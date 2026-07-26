# TMPFS 设计实现

## 前言

在此之前，尝试过使用链表来组织tmpfs的设计与实现，但是发现，实现链表来处理连续存储的内容是相当麻烦的，尤其是在关于偏移，遍历等当面的处理。

## 使用链表的问题

在使用链表的过程中，文件的偏移是一个很大的问题，读和写都是需要使用到偏移的，但是有一个问题是，你要怎么记录偏移和找到偏移？

在使用链表的过程中，想要找到对应的偏移，必须从头开始遍历到对应的偏移位置，这是一个O(n)的效率，但是如果说，只需要使用一次偏移那这个效率也是可以接受的，但是，这个偏移会一直都在用，写的时候，可能会写很大，或者删除，读之前的，都会使用到偏移，那么我们在这里就需要处理一部分问题，这个偏移的记录值，我们可以选择写在内存里一直记录，或者是选择每次都再遍历一遍，但是前者会破环当前的现有结构，而后者的效率又极其的底下，所以一个很好的办法就是使用像easyfs或者是ext4的方法，可以通过偏移在文件镜像中直接定位到地址直接读取。

而另一个问题是，如果使用纯链表组织的tmpfs，还会发现，在创建文件的时候，还需要改动目录所指向目录项的链接(我纯链表组织的tmpfs的设计是目录指向最新的目录项，并构成一个循环链表)，但是改动目录这个动作，可能还会触发一些问题，那就是如果目录的inode已经被读取到了内存中，那么inode就需要重新载入，但是当前的get_inode动作中，没有处理脏数据的动作，所以就需要我们单独再去处理inode的问题和写入tmpfs文件的问题。

## 重新设计

仿照easyfs的设计，我打算重新设计tmpfs的实现

不过，观察他们的之间的区别是，easyfs他的整个系统文件是一个整体，而tmpfs使用的是内存，是可能离散的地址，而如何将离散的地址组织起来，那就可以使用数组。

所以先设计superblock:

```c
struct tmpfs_superblock {
	uint64 magic;
	// Bitmap page pointers: 3 direct entries, 1 single-indirect root,
	// and 1 double-indirect root.
	// record the page address where the bitmap is stored
	uint64 ibitmap[TMPFS_IBIT_NUM];
	uint64 dbitmap[TMPFS_DAIT_NUM];
	// 10 direct inode addresses, 1 single-indirect root, and 1 double-indirect root.
	// record the inode addresses that are collected
	uint64 inode_collected[TMPFS_INO_NUM];
	uint64 data_collected[TMPFS_DBLK_NUM];
};
```

使用数组进行组织，在数组中存放分配页的地址，然后使用分配页中的位作为位图记录，同时增加一级间接页和二级间接页增加可用范围，当前的规划是3个直接页，1个一级间接页，1个二级间接页，这样分配的空间足够大。

同时分配页面用来存放inode和dirent，inode_collected用来存放存在的inode数据，而data_collected用来存放存储的dirent，而文件所本身拥有的数据有自身所掌控，不写入data_collected。

对于inode和dirent的设计就跟easyfs一致就可以了
```c
// sizeof(struct tmpfs_inode) = 32B
struct tmpfs_inode {
	uint16 type;
	uint16 nlinks;
	uint64 size;
	// record the page address where the data is stored
	// 10 direct data block addresses, 1 single-indirect root, and 1 double-indirect root.
	uint64 blocks[TMPFS_DBLK_NUM];
	uint32 padding[3]; // align to 32B
};

// sizeof(struct tmpfs_dir_entry) = 64B
struct tmpfs_dir_entry {
	uint64 inode_num;
	char name[56];
};
```