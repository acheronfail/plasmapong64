/* Bit-exact Q12 pressure passes. Three rotating pressure rows and one
 * divergence row fit in DMEM. The serial Gauss-Seidel left dependency stays
 * scalar; this first overlay deliberately makes no precision/solver changes.
 * Each command is one pass so RSPQ can service other overlays between passes.
 */
#include <rsp_queue.inc>
    .set noreorder
    .set at
#define ROW_BYTES 192
#define LAST_ROW 31

    .data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand PressurePass,12
RSPQ_EndOverlayHeader
RSPQ_EmptySavedState
    /* The pinned embed rule aligns blobs to 8 bytes. Pad the trailing saved
       state so a subsequent overlay's CPU registration cannot cache its DMA
       output line by reading the next blob's first instructions. */
    .quad 0
    .bss
    .align 4
rows: .ds.b ROW_BYTES*3
divrow: .ds.b ROW_BYTES
    .text

/* a0=pressure physical address, a1=divergence address, a2=pass (0..7).
 * gp belongs to RSPQ. DMAExec clobbers at/t2 and adjusts s4; all transfers
 * here are aligned and synchronous. Other registers survive DMAExec.
 */
PressurePass:
    move s1,a0                 /* pressure base */
    move s2,a1                 /* divergence cursor */
    move s3,a2                 /* pass */
    li s5,%lo(rows)            /* above */
    li s6,%lo(rows)+ROW_BYTES  /* current */
    li s7,%lo(rows)+ROW_BYTES*2 /* below */
    li t0,ROW_BYTES-1
    li t1,ROW_BYTES
    li fp,1                    /* row y */
    addiu s2,s2,ROW_BYTES
    bnez s3,load_initial
    addiu k0,s1,ROW_BYTES      /* pressure cursor */
    /* First pass starts at zero regardless of old pressure/scratch contents. */
    move t3,s5
    addiu t4,s5,ROW_BYTES
zero_above:
    sw zero,0(t3)
    addiu t3,t3,4
    bne t3,t4,zero_above
    nop
    b row_loop
    nop
load_initial:
    move s0,s1
    move s4,s5
    jal DMAExec
    li t2,DMA_IN
    move s0,k0
    move s4,s6
    jal DMAExec
    li t2,DMA_IN
row_loop:
    beqz s3,load_divergence
    nop
    addiu s0,k0,ROW_BYTES
    move s4,s7
    jal DMAExec
    li t2,DMA_IN
load_divergence:
    move s0,s2
    li s4,%lo(divrow)
    jal DMAExec
    li t2,DMA_IN
    li t3,4                    /* x byte offset */
    beqz s3,first_cells
    move t4,zero               /* zero initial left on first pass */
    lw t4,0(s6)
    /* A whole row is small enough to unroll in IMEM. Constant offsets avoid
       four address additions and loop bookkeeping for every pressure cell. */
    .set cell,4
    .rept 46
    lw t5,cell(s4)
    lw t6,cell(s5)
    lw t7,cell+4(s6)
    lw t8,cell(s7)
    addu t5,t5,t6
    addu t7,t7,t8
    addu t5,t5,t7
    addu t4,t4,t5
    sra t4,t4,2
    sw t4,cell(s6)
    .set cell,cell+4
    .endr
    b row_done
    nop
first_cells:
    /* As on later passes, constant offsets remove scalar address arithmetic.
       The initial pressure is zero, so only divergence, above and left enter. */
    .set cell,4
    .rept 46
    lw t5,cell(s4)
    lw t6,cell(s5)
    addu t5,t5,t6
    addu t4,t4,t5
    sra t4,t4,2
    sw t4,cell(s6)
    .set cell,cell+4
    .endr
row_done:
    /* Edge copies can happen now: later rows use only interior neighbours. */
    lw t5,4(s6)
    sw t4,ROW_BYTES-4(s6)
    sw t5,0(s6)
    move s0,k0
    move s4,s6
    jal DMAExec
    li t2,DMA_OUT
    li t3,1
    bne fp,t3,not_top
    nop
    move s0,s1
    jal DMAExec
    li t2,DMA_OUT
not_top:
    li t3,LAST_ROW
    bne fp,t3,next_row
    nop
    addiu s0,k0,ROW_BYTES
    jal DMAExec
    li t2,DMA_OUT
    j RSPQ_Loop
    nop
next_row:
    move t3,s5
    move s5,s6
    move s6,s7
    move s7,t3
    addiu fp,fp,1
    addiu k0,k0,ROW_BYTES
    b row_loop
    addiu s2,s2,ROW_BYTES
