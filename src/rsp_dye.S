/* Signed halfword channels (Q13 dye or Q4 velocity), Q15 fractions.
 * Sample differences must fit signed 16-bit lanes. One channel per command.
 * A complete 3168-byte source grid fits beside 24 traces and 24 outputs.
 * Scratch is reloaded on every command, including after overlay switches.
 */
#include <rsp_queue.inc>
    .set noreorder
    .set at
#define GRID_BYTES (48*33*2)
#define TRACE_BYTES (24*6)
#define OUT_BYTES (24*2)
    .data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand DyeChannel,16
RSPQ_EndOverlayHeader
RSPQ_EmptySavedState
    .bss
    .align 4
grid: .ds.b GRID_BYTES
traces: .ds.b TRACE_BYTES
output: .ds.b OUT_BYTES
    .text
/* a0=source, a1=trace, a2=output, a3=rounding<<16 | decay Q15.
 * DMAExec uses s0/s4/t0/t1/t2; gp is reserved by RSPQ. */
DyeChannel:
    move s1,a1
    move s2,a2
    andi s3,a3,65535
    li s5,66                  /* 1584 / 24 chunks */
    sll t3,s3,1               /* unsigned Q16 decay; unity bypasses multiply */
    mtc2 t3,$v07.e0
    srl t3,a3,16
    mtc2 t3,$v08.e0
    vxor $v08,vzero,$v08.e0
    ctc2 zero,COP2_CTRL_VCO
    move s0,a0
    li s4,%lo(grid)
    li t0,GRID_BYTES-1
    li t1,GRID_BYTES
    jal DMAExec
    li t2,DMA_IN
chunk:
    move s0,s1
    li s4,%lo(traces)
    li t0,TRACE_BYTES-1
    li t1,TRACE_BYTES
    jal DMAExec
    li t2,DMA_IN
    li s6,%lo(traces)
    li s7,%lo(output)
    li fp,3                   /* three vectors per DMA chunk */
batch:
    /* Arbitrary gathers stay scalar-addressed; interpolation is eight-wide. */
    .set lane,0
    .rept 8
    lhu t3,lane*6(s6)
    addiu t3,t3,%lo(grid)
    lsv $v01,lane*2,0,t3
    lsv $v02,lane*2,2,t3
    lsv $v03,lane*2,96,t3
    lsv $v04,lane*2,98,t3
    lsv $v05,lane*2,lane*6+2,s6
    lsv $v06,lane*2,lane*6+4,s6
    .set lane,lane+1
    .endr
    vsub $v02,$v02,$v01
    vsub $v04,$v04,$v03
    vmulf $v02,$v02,$v05
    vmulf $v04,$v04,$v05
    vadd $v01,$v01,$v02
    vadd $v03,$v03,$v04
    vsub $v03,$v03,$v01
    vmulf $v03,$v03,$v06
    vadd $v01,$v01,$v03
    li t3,32768
    beq s3,t3,store_batch
    nop
    vmudm $v01,$v01,$v07.e0
    vmadn $v09,$v08,K1
    vsar $v01,COP2_ACC_MD
store_batch:
    sqv $v01,0,s7
    addiu fp,fp,-1
    addiu s6,s6,48
    bnez fp,batch
    addiu s7,s7,16
    move s0,s2
    li s4,%lo(output)
    li t0,OUT_BYTES-1
    li t1,OUT_BYTES
    jal DMAExec
    li t2,DMA_OUT
    addiu s5,s5,-1
    addiu s1,s1,TRACE_BYTES
    bnez s5,chunk
    addiu s2,s2,OUT_BYTES
    j RSPQ_Loop
    nop
