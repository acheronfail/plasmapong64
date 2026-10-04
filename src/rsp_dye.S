/* Signed halfword channels (Q13 dye or Q4 velocity), Q15 fractions.
 * Sample differences must fit signed 16-bit lanes. One channel per command.
 * A complete 3168-byte source grid fits beside 48 traces; their consumed
 * storage is reused for the 48 outputs.
 * Scratch is reloaded on every command, including after overlay switches.
 */
#include <rsp_queue.inc>
    .set noreorder
    .set at
#define GRID_BYTES (48*33*2)
#define TRACE_BYTES (48*6)
#define OUT_BYTES (48*2)
    .data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand DyeChannel,16
RSPQ_EndOverlayHeader
RSPQ_EmptySavedState
    /* The pinned embed rule aligns blobs to 8 bytes. Pad the trailing saved
       state so a subsequent overlay's CPU registration cannot cache its DMA
       output line by reading the next blob's first instructions. */
    .quad 0
    .bss
    .align 4
grid: .ds.b GRID_BYTES
traces: .ds.b TRACE_BYTES
    /* Each eight-lane batch consumes 48 trace bytes before writing 16 output
       bytes at the front. Output never reaches an unread trace. */
    .set output,traces
    .text
/* a0=source, a1=trace, a2=output, a3=rounding<<16 | decay Q15.
 * DMAExec uses s0/s4/t0/t1/t2; gp is reserved by RSPQ. */
DyeChannel:
    move s1,a1
    move s2,a2
    andi s3,a3,65535
    li s5,33                  /* 1584 / 48 chunks */
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
#ifdef PLASMAPONG_ADVECTION_PIPELINE
    /* Alternate register banks: interpolate one vector while gathering the
       next. Independent scalar/vector instructions hide load/result latency.
       All three lerps and decay retain their original rounding/order. */
.macro gather a,b,c,d,tx,ty
    /* Four independent addresses cover scalar load latency. */
    lhu t3,0(s6)
    lhu t4,2(s6)
    lhu t5,4(s6)
    lhu t6,6(s6)
    addiu t3,t3,%lo(grid)
    addiu t4,t4,%lo(grid)
    addiu t5,t5,%lo(grid)
    addiu t6,t6,%lo(grid)
    lsv \a,0,0,t3
    lsv \b,0,2,t3
    lsv \c,0,96,t3
    lsv \d,0,98,t3
    lsv \a,2,0,t4
    lsv \b,2,2,t4
    lsv \c,2,96,t4
    lsv \d,2,98,t4
    lsv \a,4,0,t5
    lsv \b,4,2,t5
    lsv \c,4,96,t5
    lsv \d,4,98,t5
    lsv \a,6,0,t6
    lsv \b,6,2,t6
    lsv \c,6,96,t6
    lsv \d,6,98,t6
    /* Four independent addresses cover scalar load latency. */
    lhu t3,8(s6)
    lhu t4,10(s6)
    lhu t5,12(s6)
    lhu t6,14(s6)
    addiu t3,t3,%lo(grid)
    addiu t4,t4,%lo(grid)
    addiu t5,t5,%lo(grid)
    addiu t6,t6,%lo(grid)
    lsv \a,8,0,t3
    lsv \b,8,2,t3
    lsv \c,8,96,t3
    lsv \d,8,98,t3
    lsv \a,10,0,t4
    lsv \b,10,2,t4
    lsv \c,10,96,t4
    lsv \d,10,98,t4
    lsv \a,12,0,t5
    lsv \b,12,2,t5
    lsv \c,12,96,t5
    lsv \d,12,98,t5
    lsv \a,14,0,t6
    lsv \b,14,2,t6
    lsv \c,14,96,t6
    lsv \d,14,98,t6
    lqv \tx,16,s6
    lqv \ty,32,s6
.endm
    gather $v01,$v02,$v03,$v04,$v05,$v06
    addiu s6,s6,48
.macro stream a,b,c,d,tx,ty,na,nb,nc,nd,ntx,nty
    vsub \b,\b,\a
    lhu t3,0(s6)
    lhu t4,2(s6)
    lhu t5,4(s6)
    lhu t6,6(s6)
    addiu t3,t3,%lo(grid)
    vsub \d,\d,\c
    addiu t4,t4,%lo(grid)
    addiu t5,t5,%lo(grid)
    addiu t6,t6,%lo(grid)
    lsv \na,0,0,t3
    lsv \nb,0,2,t3
    vmulf \b,\b,\tx
    lsv \nc,0,96,t3
    lsv \nd,0,98,t3
    lsv \na,2,0,t4
    lsv \nb,2,2,t4
    lsv \nc,2,96,t4
    vmulf \d,\d,\tx
    lsv \nd,2,98,t4
    lsv \na,4,0,t5
    lsv \nb,4,2,t5
    lsv \nc,4,96,t5
    lsv \nd,4,98,t5
    vadd \a,\a,\b
    lsv \na,6,0,t6
    lsv \nb,6,2,t6
    lsv \nc,6,96,t6
    lsv \nd,6,98,t6
    lhu t3,8(s6)
    vadd \c,\c,\d
    lhu t4,10(s6)
    lhu t5,12(s6)
    lhu t6,14(s6)
    addiu t3,t3,%lo(grid)
    addiu t4,t4,%lo(grid)
    vsub \c,\c,\a
    addiu t5,t5,%lo(grid)
    addiu t6,t6,%lo(grid)
    lsv \na,8,0,t3
    lsv \nb,8,2,t3
    lsv \nc,8,96,t3
    vmulf \c,\c,\ty
    lsv \nd,8,98,t3
    lsv \na,10,0,t4
    lsv \nb,10,2,t4
    lsv \nc,10,96,t4
    lsv \nd,10,98,t4
    vadd \a,\a,\c
    lsv \na,12,0,t5
    lsv \nb,12,2,t5
    lsv \nc,12,96,t5
    lsv \nd,12,98,t5
    lsv \na,14,0,t6
    lsv \nb,14,2,t6
    lsv \nc,14,96,t6
    lsv \nd,14,98,t6
    lqv \ntx,16,s6
    lqv \nty,32,s6
    li t3,32768
    beq s3,t3,1f
    nop
    vmudm \a,\a,$v07.e0
    vmadn $v09,$v08,K1
    vsar \a,COP2_ACC_MD
1:
    sqv \a,0,s7
    addiu s7,s7,16
    addiu s6,s6,48
.endm
    stream $v01,$v02,$v03,$v04,$v05,$v06,$v11,$v12,$v13,$v14,$v15,$v16
    stream $v11,$v12,$v13,$v14,$v15,$v16,$v01,$v02,$v03,$v04,$v05,$v06
    stream $v01,$v02,$v03,$v04,$v05,$v06,$v11,$v12,$v13,$v14,$v15,$v16
    stream $v11,$v12,$v13,$v14,$v15,$v16,$v01,$v02,$v03,$v04,$v05,$v06
    stream $v01,$v02,$v03,$v04,$v05,$v06,$v11,$v12,$v13,$v14,$v15,$v16
    /* Last vector has no next gather and must not read beyond this chunk. */
    vsub $v12,$v12,$v11
    vsub $v14,$v14,$v13
    vmulf $v12,$v12,$v15
    vmulf $v14,$v14,$v15
    vadd $v11,$v11,$v12
    vadd $v13,$v13,$v14
    vsub $v13,$v13,$v11
    vmulf $v13,$v13,$v16
    vadd $v11,$v11,$v13
    li t3,32768
    beq s3,t3,2f
    nop
    vmudm $v11,$v11,$v07.e0
    vmadn $v09,$v08,K1
    vsar $v11,COP2_ACC_MD
2:
    sqv $v11,0,s7
#else
    li fp,6                   /* six vectors per DMA chunk */
batch:
    /* Arbitrary gathers stay scalar-addressed; interpolation is eight-wide. */
    .set lane,0
    .rept 8
    lhu t3,lane*2(s6)
    addiu t3,t3,%lo(grid)
    lsv $v01,lane*2,0,t3
    lsv $v02,lane*2,2,t3
    lsv $v03,lane*2,96,t3
    lsv $v04,lane*2,98,t3
    .set lane,lane+1
    .endr
    lqv $v05,16,s6
    lqv $v06,32,s6
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
#endif
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
