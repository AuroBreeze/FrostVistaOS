#include <user/user.h>
#include <stdarg.h>

#define RADIX_DEC 10
#define RADIX_HEX 16

static const char digits[] = "0123456789abcdef";

struct output_buffer {
	char data[256];
	unsigned long count;
};

static void write_output(const char *data, unsigned long count)
{
	while (count > 0) {
		long written = write(STDOUT_FILENO, data, count);
		if (written <= 0)
			return;
		data += written;
		count -= written;
	}
}

void print(const char *str)
{
	write_output(str, strlen(str));
}

static void flush_output(struct output_buffer *out)
{
	write_output(out->data, out->count);
	out->count = 0;
}

static void append_char(struct output_buffer *out, char c)
{
	if (out->count == sizeof(out->data))
		flush_output(out);
	out->data[out->count++] = c;
}

static void print_unsigned(struct output_buffer *out, uint64 value, uint base)
{
	char buf[32];
	int count = 0;

	do {
		buf[count++] = digits[value % base];
		value /= base;
	} while (value != 0);

	while (count > 0)
		append_char(out, buf[--count]);
}

static void print_signed(struct output_buffer *out, int value)
{
	uint magnitude = (uint) value;

	if (value < 0) {
		append_char(out, '-');
		/* Unsigned subtraction also handles the most negative int. */
		magnitude = 0U - magnitude;
	}
	print_unsigned(out, magnitude, RADIX_DEC);
}

static void print_pointer(struct output_buffer *out, const void *ptr)
{
	uint64 value = (uint64) ptr;

	append_char(out, '0');
	append_char(out, 'x');
	for (int shift = 60; shift >= 0; shift -= 4)
		append_char(out, digits[(value >> shift) & 0xf]);
}

void printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	struct output_buffer out = {.count = 0};
	for (unsigned long i = 0; fmt[i] != '\0'; i++) {
		if (fmt[i] != '%') {
			append_char(&out, fmt[i]);
			continue;
		}

		char c = fmt[++i];
		if (c == '\0') {
			append_char(&out, '%');
			break;
		}

		switch (c) {
		case 'd':
			print_signed(&out, va_arg(ap, int));
			break;
		case 'x':
			print_unsigned(&out, va_arg(ap, uint), RADIX_HEX);
			break;
		case 'l':
			if (fmt[i + 1] == 'u') {
				i++;
				print_unsigned(&out, va_arg(ap, unsigned long),
					       RADIX_DEC);
			} else {
				append_char(&out, '%');
				append_char(&out, 'l');
			}
			break;
		case 'p':
			print_pointer(&out, va_arg(ap, void *));
			break;
		case 's': {
			const char *s = va_arg(ap, const char *);
			if (!s)
				s = "(null)";
			while (*s)
				append_char(&out, *s++);
			break;
		}
		case 'c':
			append_char(&out, (char) va_arg(ap, int));
			break;
		case '%':
			append_char(&out, '%');
			break;
		default:
			append_char(&out, '%');
			append_char(&out, c);
			break;
		}
	}

	va_end(ap);
	flush_output(&out);
}
