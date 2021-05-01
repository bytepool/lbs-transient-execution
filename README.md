# An Overview of Transient Execution Vulnerabilities

## Introduction

### CPU features

#### Out-of-order Execution

#### Speculative Execution

#### Branch Prediction

#### CPU internal buffers


### Attacker model(s)


### Relevant Cache Side Channel Attacks

#### Flush+Reload

#### Evict+Reload

#### ...


## Spectre & Meltdown

### Spectre v1 - Bounds Check Bypass (BCB)

### Spectre v2 - Branch Target Injection (BTI)

### Meltdown v3 - Rogue Data Cache Load (RDCL)

### Spectre-NG v3a - Rogue System Register Read (RSRR)

### Spectre-NG - Lazy FP State Restore

### Spectre-NG v1.1 - Bounds Check Bypass Store (BCBS)

### Spectre-NG v4 - Speculative Store Bypass (SSB)

### SpectreRSB - Return Mispredict

### Spectre SWAPGS


## Foreshadow - L1 Terminal Fault

### Foreshadow

### Foreshadow-OS

### Foreshadow-VMM


## Microarchitectural Data Sampling

### RIDL - Zombieload - Microarchitectural Fill Buffer Data Sampling (MFBDS)

### RIDL - Microarchitectural Load Port Data Sampling (MLPDS)

### RIDL - Microarchitectural Data Sampling Uncacheable memory (MDSUM)

### RIDL - Fallout - Microarchitectural Store Buffer Data Sampling (MSBDS)

### RIDL - Zombieload v2 - Transactional Asynchronous Abort (TAA)

### RIDL - CacheOut - L1D Eviction Sampling (L1DES)

### RIDL - Vector Register Sampling (VRS)

### CROSSTalk - Special Register Buffer Data Sampling (SRBDS)


## Load-Value Injection

