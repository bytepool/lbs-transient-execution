# Report

## Introduction & Background

With the discovery of Spectre and Meltdown in 2017, a new class of vulnerabilities was born: transient execution vulnerabilities. These are also known as speculative execution vulnerabilities or microarchitectural CPU vulnerabilities. As computer scientists or programmers we are used to working with abstractions, and as security professionals we know that vulnerabilities are often found were abstractions meet implementations. This holds true for transient execution vulnerabilities as well: programs that are perfectly secure under the typical CPU abstraction that assumes deterministic execution are decidedly less so when they meet actual CPU implementations that use out-of-order execution, speculative execution and branch prediction. Since these vulnerabilities depend on specific CPU features, different CPU architectures and models are affected to very different degrees.

In addition to the different Spectre and Meltdown variants, another class of transient execution vulnerabilities was found later, commonly known as microarchitectural data sampling (MDS) or rogue in-flight data load (RIDL). These use various CPU internal buffers to leak data.

In this work, we attempt to give an overview of the different transient execution vulnerabilities that have been found since 2017, including a brief description of the CPU features that are being exploited and the attacker model that is assumed. We also develop Proof of Concept programs that demonstrate some aspects of the vulnerabilities in a new setting or for new applications. We focus on Intel (i7, i5) processors because many known CPU vulnerabilites target them specifically and because we have them at hand.

This report is organized as follows. The rest of this section gives an overview of the key CPU features that make them succeptible to transient execution vulnerabilites. Section "Known transient execution vulnerabilities" describes some of the vulnerabilites in detail. And finally, section "Case Studies" describes the vulnerabilites we focus on in more detail as well as the Proof-of-Concept program that we develop.

### CPU features for performance optimization

Since memory access is orders of magnitude slower than modern CPUs, CPU manufacturers have come up with clever performance optimizations to effectively use the time while waiting for memory access operations to complete. These performance optimizations include the CPU features discussed in this section, and without these features many tasks run significantly slower, in some cases even 10x slower. However, it is also these optimizations that have lead to the vulnerabilities discussed in this report.

The Spectre and Meltdown variants in particular rely on out-of-order execution, speculative execution and branch prediction to leak CPU internal information to an attacker. The MDS or RIDL vulnerabilities that were found later additionally rely on various CPU internal buffers to leak secrets across CPU threads (called hyper-threads in Intel architectures).

In the following subsections, we will shortly outline what these CPU features do and how they work.

#### Out-of-order Execution, Speculative Execution and Branch Prediction

As programmers, we think of CPU instructions as being executed sequentially, one by one, but in practice instructions can be executed out-of-order, aptly named *out-of-order execution*. This happens when one or more previous instructions are waiting to complete, and the following instructions have no data dependency on the previous instructions. Simply put, if instructions don't need to know any of the resulting values of the previous instructions, they are ready to execute and the CPU will put them in a pipeline.

Note that we specifically qualify the instruction dependency to data dependencies, because control flow dependencies might be ignored if speculative execution is allowed. *Speculative execution* takes the idea of using the waiting time effectively even further, by allowing the execution of branches without knowing if they will be taken or not, i.e., executing them speculatively. This is where *branch prediction* comes in: based on previous executions, the CPU guesses which branch will be executed next.

The important part is that speculative execution is CPU internal, nothing that is executed speculatively is written back to memory, and if it turns out that a branch was taken in error, the CPU can roll back its internal state to before the branch was taken. However, speculative execution has side-effects such as affecting the CPU caches that are not rolled back, so that some information of the speculated branches can leak through side-channels.

#### Simultaneous Multithreading (SMT) & CPU internal buffers

Another speed optimization that CPU manufacturers introduced to modern architectures is so called *simultaneous multithreading (SMT)*, which makes a single CPU core look like multiple CPUs to the operating system because this allows additional CPU internal optimizations. Intel's implementation of SMT is called *Hyper-Threading Technology (HTT)*. Threads share certain internal resources, such as caches and buffers, which makes it possible for information to leak from one thread to another through side-channels that use the shared resources.

On Intel architectures, there is a buffer called the *Line Fill Buffer (LFB)*, which is a central component in most load and store operations. In the following we briefly look at what the *Line Fill Buffer (LFB)* is and what it does, because this is necessary background to understand the related MFBDS vulnerability which we explain in some detail later. There are also other interesting buffers in use on Intel architectures which are used in similar vulnerabilities, such as the *store & forward buffer* and the *load port buffer*, but we focus only on the LFB here.

The line fill buffer (LFB) is, among other things, used to temporarily store memory addresses that were not found in cache and are therefore being fetched from memory. This increases performance because several addresses can be requested to be fetched from memory at the same time without having to wait for the result. According to [1], the LFB has a number of additional tasks:

- Load squashing. If there are several outstanding requests to the same address, the LFB will point to the same LFB entry without allocating a new one.
- Write combining. If several writes are recorded to the same address, the LFB will combine them before passing them on to the higher memory levels.
- Non-temporal requests. If memory is not supposed to be cached, load and store operations will be run exclusively through the LFB.

Note that the LFB and other buffers mentioned here are unique to Intel microarchitectures.

1. <https://mdsattacks.com/files/ridl.pdf>


### Relevant Cache Side-Channel Attacks

Side-channel attacks do not directly attack the application that is being targeted, instead they derive secret information from side-effects of executing that application. The classic example would be a timing side-channel of a poorly implemented password check: if the password is compared character for character, every character that is guessed correctly increases the time the application needs to return with the answer, so an attacker can determine the characters one by one based on the timing of the applications response.

There are many types of side-channel attacks, but in the context of transient execution vulnerabilities, cache side-channel attacks are the most relevant, so we will explain some of the most important concepts here.

#### Basics of CPU caches

As you are probably aware, most desktop & server CPUs have a three-tiered hierarchy of caches, with each cache level being slightly slower and slightly bigger than the previous one: *L1*, *L2*, *L3*. The L1 cache is also typically split into a data cache (*L1d*) and an instruction cache (*L1i*). For our purposes, it is sufficient to know that any value found in cache is significantly faster to retrieve than any value that has to be fetched from main memory, and the difference in timing is clearly observable when measured.

It is also important to remember that caches are organized into so called *cache lines*, a data row of fixed size. A memory address is typically split into three parts to determine its place in the cache: [*tag* | *index* | *offset*]. A *way* is the CPU parameter that indicates how many cache lines can be stored per cache entry. For example, a 2-way set-associative cache can store 2 cache lines at the same index.

#### Flush+Reload

The basic idea of a flush+reload attack is simple: the attacker identifies a specific shared memory location they want to monitor, for instance a specific line of code in a shared library, and uses the CPU instruction *clflush* to flush the corresponding cache line from memory, thus guaranteeing that it is no longer in cache. The attacker triggers the loading of the secret by the victim process, and finally they measure the time it takes to reload the flushed cache line. If the access is quick, the attacker knows that the cache line was accessed by the victim process since it was flushed.

Some of the modern processors provide a very precise way to measure the execution time. For example on x86 there is `rdtsc` instruction. On ARM getting precise time measurements [can be more challenging](https://www.virusbulletin.com/virusbulletin/2018/07/does-malware-based-spectre-exist/#h3-challenges-armv7-poc), and the attacker might need to put some extra effort and, e.g. create its own timer by running a counter process in another thread. The precision of such timers is affected by events like context switches and hardware interrupts. When possible, the attacker can ask the OS scheduler to distribute its process and the victim process in a certain way between the CPU cores (but this measure makes more sense for PoC code than for a real attack).

#### Evict+Reload

Evict+reload is a variation of the flush+reload attack which does not use the *clflush* instruction and instead evicts the targeted cache line by accessing data that will replace it. So the main difference is that instead of using a CPU instruction to flush the cache line, the cache line is evicted by loading a different cache line in the same location. This is especially useful when an attacker does not have access to a flush instruction, for instance when performing attacks in a browser.


#### Prime+Probe

In a prime+probe attack, the attacker continuously and systematically loads its own known data into every cache line in the cache (that is, accesses every *set* and every *way*) and measures the access time for each cache set, which is known as the priming step. Next, the attacker triggers the victim process, which will load new cache lines and evict some of the attackers data from the cache. Now the probe phase works exactly the same way as the prime phase, i.e., every cache line is accessed, and the access time is measured. If the access time for a particular cache set takes longer than before, the attacker knows that the victim process accessed that particular cache set.


### Attacker model(s)

Looking at transient execution vulnerabilities, a relevant question is under which circumstances they can be exploited. Let us establish some terminology first:
- There is a *secret* which the *attacker* wants to leak, for instance the root password hash stored in /etc/shadow.
- The secret is loaded into memory by a *victim process*, for instance by an ssh server.
- The attacker can execute an *exploit* that implements a side-channel attack.

In general, the exploit must be executed on the same CPU core or thread as the victim process, and in most cases the exploit must also be able to control when the victim process loads the secret, for instance by attempting to login to the ssh server.

## Known transient execution vulnerabilities

In this section, we briefly outline the different types of transient execution vulnerabilities that have been found so far, before we dive deeper into a small selection of them later.

### Spectre & Meltdown Variants

Meltdown and Spectre allow bypassing the standard OS and VM privilege
separation mechanisms. They do so by exploiting CPU cache, speculative
execution and branch prediction. Meltdown is especially tailored to exploit a
specific implementation of OS kernel context switching, which was already fixed
in modern OSes. Spectre, unlike Meltdown, is more versatile and can be used to
bypass both OS and VM boundaries (including JavaScript VM in browsers). Its v1
version merely passively exploits the branch predictor, and Spectre v2 proposes
a technique to actively poison the branch predictor data and make the target
code do branch predictions in a certain, useful to the adversary, way.

Below is the list of vulnerabilities of this kind. We focus on the first three
and describe them in detail in the following, and we only mention the others
for completeness.

- Meltdown (aka Spectre v3) - Rogue Data Cache Load (RDCL)
- Spectre v1 - Bounds Check Bypass (BCB)
- Spectre v2 - Branch Target Injection (BTI)
- Spectre-NG v3a - Rogue System Register Read (RSRR)
- Spectre-NG - Lazy FP State Restore
- Spectre-NG v1.1 - Bounds Check Bypass Store (BCBS)
- Spectre-NG v4 - Speculative Store Bypass (SSB)
- SpectreRSB - Return Mispredict
- Spectre SWAPGS

#### Meltdown - Rogue Data Cache Load (RDCL)

When a process running on a multi-user OS (like Linux or Windows) wants to
read a file or send some data over network, it does so through a _system call_
(syscall). It puts the syscall parameters into the CPU registers and executes
a special CPU instruction (`int 0x80` or `sysenter` on x86, `cwc` or `swi`
on ARM), which triggers a CPU interrupt and starts the interrupt handler in
the OS code. The interrupt handler runs with kernel privileges, usually it
can access any memory of any process or talk to any hardware directly???the code
inside the kernel is trusted. This allows the interrupt handler to serve the
syscall request from the user process; for example, if the process requested to
read 10 bytes from some file, the interrupt handler will talk to the HDD, read
the bytes, and copy them into the buffer inside the process memory. Then the
interrupt handler restores the CPU to the state where it has been when the user
process issued the syscall, and continues executing the user process code (after
dropping the kernel privileges, of course).

In order for the kernel code to work, it needs the kernel memory (its code,
private data, drivers to work with devices) to be mapped into the address
space. But we don't want to keep this memory mapped after the interrupt has
terminated (the user might read kernel data!). We could map and unmap the kernel
memory at each syscall, but it's too expensive because of the specifics of how
mapping works. Therefore, operating systems decided to keep it mapped the whole
time, but disable any access (read, write, execute) to that data via special
permission bits which are part of the mapping settings. Changing the permission
bits takes way less time than mapping/unmapping some pages. So the syscall
handling algorithm looks like this:

1. Set the permission bits to make kernel memory readable, writable, executable
2. Run the syscall code
3. Unset the permission bits from step 1
4. Return execution to the user process who issued syscall

Meltdown takes advantage of speculative execution and cache side-channels to
read kernel memory, bypassing the permission bits.

If the user process tries to read the kernel memory, this will cause another
kind of interrupt which basically tells the kernel "this process is reading the
memory it's not allowed to read", and the kernel may either kill the process
or send it a signal to notify that it's not allowed to read that memory. Or
this is how it was supposed to work in theory. In practice, the speculative
execution manages to perform the memory access to the restricted memory and
maybe execute a couple of other instructions that come next. Of course, after
the CPU sees that the memory is not allowed to be read, it will roll back the
speculatively executed operations and return the CPU registers to the state they
had before. But it will not roll back the cache states.

There are two typical ways to exploit this vulnerability:

1. _Flush and Reload._ The attacker flushes the CPU cache to ensure that her
   data is not in the cache, runs the code that depends on the sensitive data
   inside the kernel, and then measures that access time to her data to see if
   the sensitive code has accessed her data. For example, the code could look
   like this:

   ```c
   int local[10];

   int main(){
     // ???
     
     // ??? flush the local from CPU cache here
     
     // some pointer to kernel memory that we want to read
     int *ptr = ???;
     
     // This like will cause SIGSEGV signal, because the CPU does not allow
     // us to read the *ptr. The SIGSEGV will cause the interrupt handler to
     // run. For us it will look as if this line did not run, but the CPU will
     // actually run it and mess with the CPU cache states.
     int a = local[*ptr];
   }
   
   void interrupt_handler(){
     // Measure access time to each element of local to determine which element
     // of local is in the cache. If, say, local[5] is in the cache, then we
     // know that *ptr == 5.
   }
   ```

2. Prime and Probe. This one works similarly, but now instead of flushing we
   fill the cache with our data, and then run the sensitive code to see if it
   will displace our data from cache.

   For this type of attack, one must mind the
   [cache associativity](https://www.sciencedirect.com/topics/computer-science/set-associative-cache).
   CPUs usually don't allow any cache line from the memory to be stored in any
   cache block of the CPU, but each cache line has a set of CPU cache blocks
   where it can be stored (for one- and two-way associative caches these sets
   have size 1 and 2 respectively). Knowing the cache associativity of the CPU
   lets the attacker predict which memory accesses of the sensitive code can
   displace which parts of her own data from the cache.

Modern operating systems mitigate the Meltdown attack by [not keeping the
kernel memory mapped in the user
processes](https://en.wikipedia.org/wiki/Kernel_page-table_isolation). This
introduces a considerable performance overhead for syscalls. When the kernel
memory is not mapped, the speculative execution will not load anything into the
cache, and the attack will not work.

#### Spectre v1 - Bounds Check Bypass (BCB)

Similarly to Meltdown, Spectre v1 also relies on the CPU cache and speculative
execution. But in addition to that it also exploits the branch prediction
mechanism.

Imagine that the OS kernel (or another code running on a higher privilege
level) runs this piece of code (adapted from [Linaro
blog](https://www.linaro.org/blog/meltdown-spectre/)):

```c
if (x < array1_size)
    y = array2[array1[x]]
```

If all the values in `array2` are valid indices to address `array1`, this code
normally should be safe; even if the adversary can choose an arbitrary values
of `x` as an input.

When the `x` is not in CPU cache during the execution of the above snippet,
the comparison will take time to load the value into the cache. But this won't
stop the speculative execution, it will try to keep running the code. But what
code? The `if` body or the code after it? The CPU will ask its branch predictor
to decide, and the latter will answer based on how often the `if` body was
executed in the past.

So if the adversary is sure that the branch predictor will advice the CPU to
run the `if` body, she can pass such value for `x` that `array1 + x` address
points to memory she's interested in. And it will cause a memory access to
`array2 + *(array1 + x)`, which the attacker can detect using the standard
Meltdown techniques; both "Prime and Probe" and "Flush and Reload" can work.

This attack, if successfully implemented, can target pretty much any piece
of code that is running with higher privileges: OS kernel, virtualization
hypervisor, your programming language VM (Java, JS). But, in practice, this
attack is considered to be very difficult to implement: guessing what branch
predictor will do and looking for the right piece of code in the target program
both add a lot of complexity. The Spectre v2 improves this attack with a better
way to deal with branch predictor, making it a lot more practical.

#### Spectre v2 - Branch Target Injection (BTI)

The branch predictor keeps track of how many times each branch was taken. But,
of course, it would be impossible to have a counter inside CPU for each
location in RAM which can have a branch in it (modern computers can have tens
of gigabytes of memory; if you want a counter for each memory location, you
will have to have a few gigabytes of memory in your CPU branch predictor).

Therefore, branch predictor accumulates the statistics for multiple branches
in one counter. The mapping there is similar to the mapping of set associative
cache, no separation is enforced between different privilege levels.

It means, that an attacker can create some branches in her memory that will
share branch predictor counters with the target code, and then run those
branches multiple times to bias the counters (this is the "injection" part) in
the needed direction. This way, the attacker can ensure that the needed branch
will be taken by the target code from Spectre v1.

The possibility of this attack comes from the fact that branch predictor
counters are shared between different privilege levels. Modern processors
provide no way of isolating them or manually clearing the branch
predictor counters. While everyone is waiting for CPU manufacturers
to design new processors will will have these issues addressed,
engineers from Google proposed a completely software mitigation called
[retpolines](https://support.google.com/faqs/answer/7625886) (return +
trampoline).

Retpoline technique shows how to replace any branching instruction by a few
other instructions which will behave the same and at the same time confuse
that branch predictor and not let it see that this is a branch. This fools the
speculative execution which thinks that some useless code (like infinite cycle)
is going to be executed next, while in reality control flow gets redirected
somewhere else.


### Foreshadow - L1 Terminal Fault

The initial Foreshadow attack was designed to bypass the [Intel
SGX](https://en.wikipedia.org/wiki/Software_Guard_Extensions) (Software Guard
eXtensions). Intel SGX provides a private execution environment, where a
userspace application can keep its secret data (anything from cryptographic
keys to DRM authentication data) and work with it, without allowing even a more
privileged code, like OS kernel, to access the private data. The attack relied
on the speculative execution. The later Foreshadow-OS and Foreshadow-VM attacks
elevated the Foreshadow technique to read memory which the process normally is
not authorized to read. Foreshadow-OS abuses certain features of page mapping
mechanism, and Foreshadow-VM allows reading values that other code has loaded
into L1 cache. The latter means that the attacker can potentially read some
data that belongs to a process running in another VM, if the two VMs share a
CPU core.

The following variants of this vulnerability have been assigned a CVE:

- Foreshadow
- Foreshadow-OS
- Foreshadow-VMM


### Microarchitectural Data Sampling

In this section, we discuss a new class of transient execution vulnerabilities which were named "microarchitectural data sampling (MDS)" by Intel. Different variations of vulnerabilities in this class have been named "Zombieload" or "rogue in-flight data load (RIDL)" by different groups of researchers who discovered them.

Unlike Spectre and Meltdown, MDS vulnerabilities do not use specific memory addresses to leak data, but instead use clever techniques to leak data that is being held in various Intel CPU internal buffers. This means that the attacker has limited control over which data is leaked and therefore needs to do additional filtering of the leaked data to find the information they are after. On the other hand, it is harder to mitigate these vulnerabilities.

Since the buffers and related speculative behavior are unique to Intel architectures, these vulnerabilities do not seem to work directly on other microarchitectures.

The following variants of this vulnerability have been assigned a CVE:

- Microarchitectural Fill Buffer Data Sampling (MFBDS) / Zombieload / RIDL
- Microarchitectural Load Port Data Sampling (MLPDS) / RIDL
- Microarchitectural Store Buffer Data Sampling (MSBDS) / Fallout
- Microarchitectural Data Sampling Uncacheable memory (MDSUM)
- Transactional Asynchronous Abort (TAA) / Zombieload v2 / RIDL
- L1D Eviction Sampling (L1DES) / RIDL / CacheOut
- Vector Register Sampling (VRS) / RIDL
- Special Register Buffer Data Sampling (SRBDS) / CROSSTalk


### Load-Value Injection (LVI)

Load-Value Injection (LVI) is one of the latest transient execution vulnerabilities [1]. Some researchers wondered if you can turn the findings of Meltdown around: is it possible to inject poisoned data into the speculative execution of a victim? It turns out that that is possible, and by cleverly injecting poisoned data to hijack the control flow of the victim's speculative execution, it is possible to leak secrets of the victim!

1. <https://lviattack.eu/lvi.pdf>


## Case Studies

For a case study, we focused on _Microarchitectural Fill Buffer Data Sampling_ (MFBDS / Zombieload / RIDL). We

1. describe the vulnerability in more detail here;
2. adapt its original PoC implementation to work on our machine;
3. modigy the original PoC to attack a new program in a new setting;
4. discuss possible mitigations.

The rest of this section is organized as outlined by the list above.

Originally, we were planning to include Meltdown and Spectre (v1 - v3) in our case studies as well. One of our ideas was to study the [retpolines](https://support.google.com/faqs/answer/7625886), a smart mitigation technique that replaces each branching instruction by a special gadget which implements the same branching but also "hides" the branching from the Branch Prediction Unit effectively disabling the branch predition for a specific piece of code and making Spectre vulnerabilites infeasible. Although, after spending some time reading up on it we discovered that???since Spectre has been known for a while now???this topic is extremely well studied, the retpolines are already integrated into major C compilers and used [1] by big projects like Linux Kernel, and there is already a lot of data [2] on the performance overhead of retpolines. We left the detailed description of Meltdown and Spectre in "Known transient execution vulnerabilities" section, and in the rest of this section we focus only on Zombieload/RIDL vulnerability.

1. <https://www.kernel.org/doc/html/latest/admin-guide/hw-vuln/spectre.html#kernel-mitigation>
2. <https://www.phoronix.com/scan.php?page=article&item=linux-retpoline-benchmarks>

### Microarchitectural Fill Buffer Data Sampling (MFBDS) / Zombieload / RIDL

In the following, we focus on the first variant of MDS which uses the line fill buffer (LFB) to leak data. On Intel architectures the line fill buffer (LFB) is, among other things, used to temporarily store memory addresses that were not found in cache and are therefore being fetched from memory. This increases performance because several addresses can be requested to be fetched from memory at the same time without having to wait for the result. In some cases, data may already be available in the LFB, and the CPU will speculatively load the data and continue to execute, even though the data may be completely unrelated to the requested data. As you might expect after reading about Specter and Meltdown, this can be exploited by a clever attacker.

This vulnerability was discovered independently by several groups of researchers which lead to several papers, including Zombieload [1] and RIDL [2]. We will use the example from the RIDL [2] paper to explain the concept further:

```c
	/* Flush buffer entries */
	for ( k = 0; k < 256; ++k)
		flush(buffer + k * 1024)
	
	/* Speculatively load the secret. */
	char value = *(new_page);
	
	/* Calculate the corresponding entry. */
	char *entry_ptr = buffer + (1024 * value)
	
	/* Load that entry into the cache. */
	*(entry_ptr);
	
	/* Time the reload of each buffer entry to see which entry is now cached. */
	for (k = 0; k < 256; ++k) {
		t0 = cycles();
		*(buffer + 1024 * k)
		dt = cycles() - t0;
		
		if (dt < 100)
			++results[k];
	}
```

You will notice that this looks a lot like any other flush+reload cache side-channel for other speculative execution vulnerabilities. First you flush the cache, then you speculatively load some data, request a dependent cache entry, and finally you reload the cache to see which cache entry has been loaded so that you can infer the secret value from the cache index. The real magic happens in a single line:

```c
	char value = *(new_page);
```

In this example, demand paging is used, an OS feature that loads pages when they are needed and not before. Note that the requested *new_page* in this example is a valid page that the process has access to. As the CPU sends the request for *new_page* to be paged in, it will allocate a new LFB entry for it. However, the LFB entry has not been filled with the correct data yet, so there is stale data from previous requests in the buffer, and as the CPU speculatively executes the next line which uses the value from *new_page*, it will use the stale value that is still stored in the LFB entry, thus dereferencing a potentially completely unrelated address that is potentially from another security domain. Of course, as the correct value for *new_page* arrives, the CPU will realize that it used the wrong value and roll back the execution, but at this point the cache has already been altered and the value can be leaked! The actual details of how the LFB works are a bit more complex, but for our purposes it will suffice. If you want to know more, please see the original research papers [1,2].

There are different ways of reliably triggering this vulnerability, for instance using Intel's transactional synchronization extensions (TSX), but demand paging is the most interesting one because as a fundamental OS feature it even works in a browser.

The astute reader will have noticed that we leaked a stale value, but we had no control over which value was leaked. Therefore, careful synchronization with the victim process is needed in order for an attack to succeed. Also, additional filtering is needed in order to identify relevant pieces of data from irrelevant pieces.

Synchronizing the attack so that it leaks the sensitive data in question and not something else is highly non-trivial. The original paper [2] suggests a few ways to do this all of which rely on some features on the victim process and make it less practical. But the original PoC code, which works in a very realistic scenario, manages to accomplish it without synchronization. Instead, it tries to leak whatever it can and tries to filter out the irrelevant bits using some information about the leaked data (like specific format in which it is stored). This technique is called _mask-sub-rotate_.

_Mask-sub-rotate._ As you remember from "Introduction & Background" section, the Flush+Reload attacking technique uses uses speculative execution to load a secret and then uses secret as an index to read a value in a public array. The CPU will notice that speculation was incorrect and rollback its state, but it will not rollback the cache state, thus leaking the secret to attacker. This way, the possible domain of the secret is naturally limited by the domain of the array indices. But what if the secret is actually a big (say, 64 bit) number and we want to leak only part of it? We cannot possibly allocate an array of $2^{64}$ bits. To solve this, _mask-sub-rotate_ does the _mask_ and _rotate_:

1. `secret = secret & mask`{.c}, where `mask` has `0` bits in the positions we are not interested in;
2. `rotate = secret >> shift`{.c}, where shift specifies the position of the first interesting bit in `secret`.

Both operations are done speculatively right before using `secret` to index. They basically remove the irrelevant parts of `secret`. Another challenge of this attack is that we have no control over what value is leaked, and a lot of the time we will be leaking garbage. To filter out the relevant results, the _mask-sub-rotate_ does the _sub_ step:

1. `secret = secret - expected_suffix`{.c}, where the `expected_suffix` is the value we expect the first bits of actual secret to have.

We can repeat the above three steps multiple times to leak a long secret byte by byte. With each iteration, the length of `expected_suffix` in bits will increase as the precision of our filtering.

1. <https://zombieloadattack.com/zombieload.pdf>
2. <https://mdsattacks.com/files/ridl.pdf>


### The original MFBDS PoC: leaking /etc/shadow's first line (root hash)

The proof of concept (PoC) from the RIDL paper [1] leaked the root password hash from the /etc/shadow file [2]. In order to load the /etc/shadow file, the attacker can run a simple script [3]:

```sh
while true;
    do taskset -c 0 passwd -S $USER > /dev/null;
done;
```

With `taskset -c 0` we ensure the process is run on a CPU of our choosing. Since passwd is a setuid binary, it is allowed to continously load /etc/shadow in order to print a simple status message for this particular user. This guarantees that `/etc/shadow` passes through the Line Fill Buffer.

The first line in /etc/shadow is often around 124 characters long and typically looks something like this:

```
root:$6$iX1OHhKK$HmjVzZ4bBn1o ... PvH2V67f8UPyd3gQ4l0:17787:0:99999:7:::
```

A key observation here is that the line always starts with "`root:`", so in order to filter for the correct cache line, the attacker has to look for data which starts with that exact byte sequence. Note that on a modern Intel CPU a cache line is 64 bytes, but you can only leak 1 or 2 bytes at a time due to the nature of the flush+reload side channel (i.e., leaking the value through array indexing). Since the entire line is around 124 characters long and the cache line is 64 bytes, roughly half of the secret can be leaked by looking for the cache line that starts with "`root:`". In order to leak the second cache line, it is easiest to make assumptions about which substring terminates the root hash, and to leak the remainder of the secret from the back. In the original PoC from the RIDL authors, they assumed that the terminating substring is ":0:9999", which would work with the example above.

Now consider the following example where the last fields of the line have empty defaults:

```
root:$6$iX1OHhKK$HmjVzZ4bBn1o ... PvH2V67f8UPyd3gQ4l0:17787::::::
```

This was the case on the Archlinux machine where we tested the PoC. As you might guess, leaking the first cache line worked well, but since the ending did not match the expectations, leaking the second cache line failed. Unfortunately the original PoC had some hard coded assumptions about the string ending and a number of configuration options that did not work well on our test machine, so we adapted it so that it worked reliably for us. You can find the adapted PoC and its history in a cloned repository at [4].

An interesting observation was that the attacker process had to run on a different hyperthread (CPUs visible to the OS) than the victim process, but still on the same CPU core. If they run on the same hyperthread or on a different core, the leakage does not work.

Here are some improvement ideas of the PoC that we plan to implement if we have the time:

1. While leaking the first cache line, there is no backtracking: if we read false positivies (i.e., leak the wrong bytes), the PoC gets stuck. A simple improvement would be to add backtracking when stuck for a certain amount of rounds.
2. In order to make the PoC more robust for different systems, one could try different hash endings and try to identify the correct one.
3. The PoC relies on a specific Intel CPU feature called hardware transactions (TSX). The same PoC can be implemented without using this feature, albeit less reliably. If we have the time, we plan to implement a different version of this PoC.

[??]{}

1. <https://mdsattacks.com/files/ridl.pdf>
2. <https://github.com/vusec/ridl>
3. <https://github.com/vusec/ridl/blob/master/exploits/shadow/passwd.sh>
4. <https://github.com/bytepool/ridl-clone/blob/master/exploits/shadow/leak.c>

### A new use case for the MFBDS PoC: leaking a private ssh key

In addition to adapting the original PoC to our test machine, we also extended it to a new use case, specifically leaking a private ssh key. This use case is slightly contrived since it requires the victim to continuously load their private key, but it shows that adaptation of the PoC to new use cases is possible.

In order to continously load the private key, we tried to run another simple script [1]:

```sh
while true; do
  taskset -c 1 ssh-add -D &>/dev/null
  taskset -c 0 ssh-add ./dummy_ecdsa &>/dev/null
done
```

But this did not work reliably enough, so we gave the attacker a bit more power (initially for the testing purposes) and we instead loaded the key with `cat`{.sh}:

```sh
while true; do
  taskset -c 0 cat ./dummy_ecdsa &>/dev/null
done
```

Since a private key has a fixed header and trailer in the base64 encoded version, we can search for the corresponding cache lines, as we have done with the `/etc/shadow` root entry. In our example, this looks something like this:

```
-----BEGIN OPENSSH PRIVATE KEY-----
b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAaAAAABNlY2RzYS
qyAAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBMmdILkd/l1l5RUn
...
u1FDdd1p40QRaqC85yKTXdp/WBDbNNKXvlPCm17KAmPyTG0oOSm06Evyeg2LO29EhbATl/
0AAAAgNAPUPqtfLjkHsRTj69s3NuySrrt+/B4wV0nm3qPNKk4AAAAHYWxAY2FrZQE=
-----END OPENSSH PRIVATE KEY-----
```

In order to leak the first cache line, we adapted the shadow RIDL PoC to allow for the slightly different character set that needs to be filtered for (base64 + newline). In the original PoC, the characters are iterated over for loop, but since we needed to include line breaks and spaces, which are numerically quite a bit apart from the remaining characters, we figured it might be more convenient to iterate over an array of valid values. This naive implementation lead to another moment of enlightenment: after implementing the logic with an array of valid chars, the secret leakage became totally unreliable, seemingly leaking nonsense, which led us to conclude that the array we added interfered with the side-channel because we unwittingly added memory lookups to our attack code which messed up the LFB and the cache. So we learned that memory lookups must be avoided in the attack code at all costs. After adapting for the different valid characters, leaking the first cache line worked as intended (using the 'cat' victim process).

We also managed to successfully leak the second cache line of the key, but only if we let the adversay know a few last bytes of it (the attack needs to know some suffix to recognize the correct data in the noise). We were expecting that the same trick as the one that worked with the second cache line will work with the following cache lines, but it does not???something in the PoC code seems to be binding it to the second cache line, but we did not figure out what it is.

The PoC recovers the first two cache lines of SSH key in 29 seconds if the victim runs `cat ./dummy_ecdsa`{.sh}. We couldn't make it work for `ssh-add ./dummy_ecdsa`{.sh}. Look like there is too much noise, or it could be that `ssh-add`{.sh} loads the key in some unusual way: by aligning it in the memory not the way we expect, or reading it and parsing in smaller chunks.

1. <https://github.com/bytepool/ridl-clone/blob/master/exploits/shadow/ssh-agent.sh>
2. <https://github.com/bytepool/ridl-clone/blob/master/exploits/shadow/leak-ssh.c>

## Conclusion

In this project, we studied the existing transient execution CPU vulnerabilites, and summarized the key ones in our report???focusing on their application scenarios, limitations, mitigations and their relationship to each other.

We chose Zombieload/RIDL as a case study to focus on. We adapted its original PoC to work on our machine, and managed to leak the root hash from `/etc/shadow` by an unprivileged user.

We further modified the original PoC to leak the first two cache lines of user's SSH private key instead of root hash. The full attack takes about 29 seconds if a victim is running `cat ./dummy_ecdsa`{.sh} in a loop, albeit a lot longer if the victim is doing something more realistic like `ssh-add ./dummy_ecdsa`{.sh}.

We also planned but did not finish the following (pointers for "future work" in case future students want to work on something similar for their course project).

1. Adding backtracking to the original exploit. The exploit extracts secret byte by byte, each new iteration depending on the previously discovered bytes; and if it some points it extracts an incorrect value, it hangs. A simple backtracking check could make this exploit more robust: if we see that the current iteration is failing to find the byte for too long, we could go one byte back and try to find it again.

2. Implement the same PoC without using Intel TSX.

3. Leak more parts of the SSH key in our second PoC. Currently, we leak only the first two cache lines. And we have not figured out why the same exploit does not work for the following cache lines of the key.

4. Make the PoC works even if the victim runs `ssh-add ./dummy_ecdsa`{.sh} (unstead of using `cat`{.sh}). This will make PoC work in a more realistic scenario.
