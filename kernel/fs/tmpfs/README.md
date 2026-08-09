# TMPFS 设计实现

## 前言

在此之前，尝试过使用链表来组织tmpfs的设计与实现，但是发现，实现链表来处理连续存储的内容是相当麻烦的，尤其是在关于偏移，遍历等当面的处理。

## 使用链表的问题

在使用链表的过程中，文件的偏移是一个很大的问题，读和写都是需要使用到偏移的，但是有一个问题是，你要怎么记录偏移和找到偏移？

在使用链表的过程中，想要找到对应的偏移，必须从头开始遍历到对应的偏移位置，这是一个O(n)的效率，但是如果说，只需要使用一次偏移那这个效率也是可以接受的，但是，这个偏移会一直都在用，写的时候，可能会写很大，或者删除，读之前的，都会使用到偏移，那么我们在这里就需要处理一部分问题，这个偏移的记录值，我们可以选择写在内存里一直记录，或者是选择每次都再遍历一遍，但是前者会破环当前的现有结构，而后者的效率又极其的底下，所以一个很好的办法就是使用像easyfs或者是ext4的方法，可以通过偏移在文件镜像中直接定位到地址直接读取。

而另一个问题是，如果使用纯链表组织的tmpfs，还会发现，在创建文件的时候，还需要改动目录所指向目录项的链接(我纯链表组织的tmpfs的设计是目录指向最新的目录项，并构成一个循环链表)，但是改动目录这个动作，可能还会触发一些问题，那就是如果目录的inode已经被读取到了内存中，那么inode就需要重新载入，但是当前的get_inode动作中，没有处理脏数据的动作，所以就需要我们单独再去处理inode的问题和写入tmpfs文件的问题。

## 重新设计 - 1

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

但是在准备实现的过程中，还是遇到了部分问题，如果在不使用链表的情况下，`ibitmap`, `dbitmap` 等的间接地址都需要分配内存空间，这就会造成内存空间的浪费。那如果这样也不可以的话，tmpfs应该要怎样进行设计？

## 重新设计 - 2

2026-7-29

即使是使用easyfs的设计方法，也是有些内存无法使用的缺陷，这些缺陷的问题都是在`superblock`中的间接地址触发的，除非不使用间接地址，在最开始的tmpfs设计中，我是使用了纯链表实现，但是实现相当繁琐和麻烦，可能是我的设计问题，而这次是完全抛弃了链表，导致的内存无法完全使用的问题。

所以，这次可以改为部分链表的形式，内容的保存使用数组保存地址，而目录的搜索，查询，使用链表。

在查询资料中看到了`radix tree`, 在目录的路径形式下，有大量的路径前缀相同，所有，可以尝试使用`radix tree`来进行目录的链接，搜索等操作。

但是会遇到一个问题，如果只用`node` 的情况下，可能有多个子节点，如何处理这个动态多的子节点问题，或者使用`node`＋`edge`,也是一个动态边的问题。

我记得我设置了最大的路径长度是128, 最大文件名是14, 那么我可以使用127个固定数组来限制，因为最短文件名是1，同时数组存放指针减小体积，说实话也不小了，64位指针8B，总共1016B

## 重新设计 - 3

更加简单的思路是使用左兄弟右孩子表示法，把目录中的文件上下串起来，然后名字正常使用DIRSIZE的限制，也就是14。

但是内存数据又和磁盘数据的存放不一样，不使用inode number数据，所以，在填充inode number的时候，可以使用 addr % PGSIZE的方法，这样可以使用4096个缓存数据，而且我现在的bcache中也没有这么多缓存空间，也是完全够用的。

所以在设计上，设计vfs_inode->private的填充是tmpfs_inode，tmpfs_inode中设计回指tmpfs_dir_entry，tmpfs_dir_entry中也指向tmpfs_inode，方便寻找
