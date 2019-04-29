#include "helper.h"

using namespace std;

static int hChild;
static unsigned long long int trials, hijacktramp_addr;
static volatile int hijacked = 0;

// This function runs the child thread that attempts to hijack the main thread.
// The retaddr_ptr argument points to the main thread's return address slot.
void *ChildThread(void * retaddr_ptr)
{
    printf("Child thread started hijacking.\n");
	// Repeatedly write <hijacktramp_addr> into the main thread's return address slot.
	// The write is attempted <trials> times.
	asm (
		"    mov     %0, %%ecx\n\t"
		"L:"
        "    mov     %1, (%2)\n\t"
        "    loop L"
        :                                                           // OutputOperands   : A comma-separated list of the C variables modified
        : "r" (trials), "r" (hijacktramp_addr), "r" (retaddr_ptr)   // InputOperands    : A comma-separated list of C expressions read
        : "ecx", "memory"                                           // Clobbers         : A comma-separated list of registers or other values changed
	);
    
    usleep(100000); // 0.1 second sleep for making sure hijacked has changed if it should
    
	// If the hijack was successful, the main thread sets the global "hijacked" variable to 1.
	if (!hijacked)
	{
		printf("All trials complete.  Hijack unsuccessful.\n");
		exit(0);  // The main thread loops infinitely, so to terminate it, we use ExitProcess.
	}
	return 0;
}

int main(int argc, char* argv[])
{
	unsigned int idChild;
    pthread_t thread1;
	void * esp_addr;

	printf("Enter number of trials: ");
	scanf("%llu", &trials);
    
    int flag = 0;
    
    asm (
        "    lea     HIJACKTRAMP, %%eax\n\t"
        "    mov     %%eax, %0\n\t"
		"    lea     -4(%%esp), %%eax\n\t"		        // Store the address of the forthcoming return address in esp_addr.
		"    mov     %%eax, %1\n\t"
        "HIJACKTRAMP:"
        : "=r" (hijacktramp_addr), "=r" (esp_addr)  // OutputOperands   : A comma-separated list of the C variables modified
        :                                           // InputOperands    : A comma-separated list of C expressions read
        : "eax", "memory"                           // Clobbers         : A comma-separated list of registers or other values changed
	);
    
    if (flag) {
        hijacked = 1;
        goto HIJACKED;
    }
    
    flag = 1;
    
	hChild = pthread_create(				    // Spawn the child (hijacker) thread.
        &thread1,
        NULL,
        ChildThread,
        esp_addr);
	
    if (hChild) { 
        printf("Could not make child thread!\n");
        abort();
    }

	// Repeat a guarded return in the tightest possible infinite loop.
    
	asm (
		"    lea     L2, %ebx\n\t"
		"L1:"
        "    call    L3\n\t"
        "L2:"
        "    jmp     L1\n\t"
        "L3:"
        "    ret"
	);

	printf("Infinite loop ended normally---impossible!\n");
	abort();

	// If the hijacker is successful, the "ret" above will jump here.
HIJACKED:

    asm (
		"push $0"	// The child thread might still be running, so push a dummy return address for it to continue hijacking.
	);
    
	printf("Hijack successful!\n");
	pthread_join(thread1, NULL);   // Wait for the child thread to terminate normally.
    
	asm (
        "pop %eax"
    );  // Pop the dummy return address.
    
	return 0;
}
