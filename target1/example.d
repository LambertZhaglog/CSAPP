
example.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <.text>:
   0:	48 81 ec 00 10 00 00 	sub    $0x1000,%rsp
   7:	6a 00                	pushq  $0x0
   9:	49 ba 35 39 62 39 39 	movabs $0x6166373939623935,%r10
  10:	37 66 61 
  13:	41 52                	push   %r10
  15:	48 89 e7             	mov    %rsp,%rdi
  18:	48 81 c4 10 10 00 00 	add    $0x1010,%rsp
  1f:	68 fa 18 40 00       	pushq  $0x4018fa
  24:	c3                   	retq   
