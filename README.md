# allogator
A simple allocator that uses mmap and sbrk.

Works on Linux and OS X.

## Features
- [x] Allocate large blocks of memory via mmap
- [x] Allocate small blocks of memory via sbrk
- [x] Free memory blocks
- [ ] Squash memory blocks
- [ ] Set memory protection on memory blocks
- [ ] Force use of mmap or sbrk
- [ ] Set custom thresholds for mmap and sbrk
- [ ] Setup stack pointer for GC
- [ ] Scan heap for memory leaks
- [ ] Scan stack for memory leaks
- [ ] Automatic GC