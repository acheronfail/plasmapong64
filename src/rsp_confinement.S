/* Q4 velocity -> curl/96 -> normalized confinement -> Q4 velocity.
 * Row DMAs own full cache lines; all scratch is reloaded between commands.
 * RSPQ owns gp and v00/v30/v31. DMAExec clobbers s0/s4/t0/t1/t2/at.
 */
#include <rsp_queue.inc>
    .set noreorder
    .set at
#define ROW 96
    .data
RSPQ_BeginOverlayHeader
    RSPQ_DefineCommand Curl,12
    RSPQ_DefineCommand Confine,16
RSPQ_EndOverlayHeader
RSPQ_EmptySavedState
    /* The pinned embed rule aligns blobs to 8 bytes. Pad the trailing saved
       state so a subsequent overlay's CPU registration cannot cache its DMA
       output line by reading the next blob's first instructions. */
    .quad 0
    .bss
    .align 4
above: .ds.b ROW
below: .ds.b ROW
pad_before: .ds.b 16
middle: .ds.b ROW
pad_after: .ds.b 16
urow: .ds.b ROW
vrow: .ds.b ROW
edges: .ds.b 16
    .text
.macro row_in base, delta, buffer
    addiu s0,\base,\delta
    li s4,%lo(\buffer)
    li t0,ROW-1
    li t1,ROW
    jal DMAExec
    li t2,DMA_IN
.endm
.macro row_out base, buffer
    move s0,\base
    li s4,%lo(\buffer)
    li t0,ROW-1
    li t1,ROW
    jal DMAExec
    li t2,DMA_OUT
.endm
Curl:
    addiu s1,a0,ROW
    addiu s2,a1,ROW
    addiu s3,a2,ROW
    li s5,31
    ctc2 zero,COP2_CTRL_VCO
curl_row:
    row_in s1,-ROW,above
    row_in s1,ROW,below
    row_in s2,0,middle
    move s6,zero
    li s7,6
curl_batch:
    addiu t3,s6,%lo(above)
    addiu t4,s6,%lo(below)
    addiu t5,s6,%lo(middle)
    addiu t6,s6,%lo(urow)
    lqv $v01,0,t3
    lqv $v02,0,t4
    /* Two partial vector loads join the unaligned neighbour window. */
    addiu t8,t5,-2
    lqv $v03,0,t8
    lrv $v03,16,t8
    addiu t8,t5,2
    lqv $v04,0,t8
    lrv $v04,16,t8
    vsub $v03,$v04,$v03
    vsub $v01,$v01,$v02
    vmulf $v03,$v03,K16384
    vmacf $v03,$v01,K16384
    sqv $v03,0,t6
    addiu s7,s7,-1
    bnez s7,curl_batch
    addiu s6,s6,16
    sh zero,%lo(urow)(zero)
    sh zero,%lo(urow)+ROW-2(zero)
    row_out s3,urow
    addiu s1,s1,ROW
    addiu s2,s2,ROW
    addiu s5,s5,-1
    bnez s5,curl_row
    addiu s3,s3,ROW
    j RSPQ_Loop
    nop
Confine:
    addiu s1,a0,ROW*2
    addiu s2,a1,ROW*2
    addiu s3,a2,ROW*2
    li s5,29
    mtc2 a3,$v25.e0
    li t3,-16383
    mtc2 t3,$v26.e0
    li t3,16383
    mtc2 t3,$v27.e0
    ctc2 zero,COP2_CTRL_VCO
confine_row:
    row_in s3,-ROW,above
    row_in s3,0,middle
    row_in s3,ROW,below
    row_in s1,0,urow
    row_in s2,0,vrow
    /* Preserve x=0,1,46,47; only the interior 44 columns receive forces. */
    lw t3,%lo(urow)(zero)
    lw t4,%lo(urow)+ROW-4(zero)
    lw t5,%lo(vrow)(zero)
    lw t6,%lo(vrow)+ROW-4(zero)
    sw t3,%lo(edges)(zero)
    sw t4,%lo(edges)+4(zero)
    sw t5,%lo(edges)+8(zero)
    sw t6,%lo(edges)+12(zero)
    move s6,zero
    li s7,6
confine_batch:
    addiu t3,s6,%lo(above)
    addiu t4,s6,%lo(below)
    addiu t5,s6,%lo(middle)
    addiu t6,s6,%lo(urow)
    addiu t7,s6,%lo(vrow)
    lqv $v01,0,t3
    lqv $v02,0,t4
    lqv $v07,0,t5
    addiu t8,t5,-2
    lqv $v03,0,t8
    lrv $v03,16,t8
    addiu t8,t5,2
    lqv $v04,0,t8
    lrv $v04,16,t8
    vabs $v01,$v01,$v01
    vabs $v02,$v02,$v02
    vabs $v03,$v03,$v03
    vabs $v04,$v04,$v04
    vsub $v09,$v04,$v03
    vsub $v10,$v02,$v01
    /* Squared length is positive and <= 2*32766^2 < 2^31. */
    vmudh $v11,$v09,$v09
    vmadh $v11,$v10,$v10
    vsar $v12,COP2_ACC_HI
    vsar $v13,COP2_ACC_MD
    /* Each lookup consumes a 32-bit squared length and produces Q31 rsqrt.
       Scalar elements are pipelined independently of the eight-wide math. */
    vrsqh $v14.e0,$v12.e0
    vrsql $v15.e0,$v13.e0
    vrsqh $v14.e0,vzero.e0
    vrsqh $v14.e1,$v12.e1
    vrsql $v15.e1,$v13.e1
    vrsqh $v14.e1,vzero.e0
    vrsqh $v14.e2,$v12.e2
    vrsql $v15.e2,$v13.e2
    vrsqh $v14.e2,vzero.e0
    vrsqh $v14.e3,$v12.e3
    vrsql $v15.e3,$v13.e3
    vrsqh $v14.e3,vzero.e0
    vrsqh $v14.e4,$v12.e4
    vrsql $v15.e4,$v13.e4
    vrsqh $v14.e4,vzero.e0
    vrsqh $v14.e5,$v12.e5
    vrsql $v15.e5,$v13.e5
    vrsqh $v14.e5,vzero.e0
    vrsqh $v14.e6,$v12.e6
    vrsql $v15.e6,$v13.e6
    vrsqh $v14.e6,vzero.e0
    vrsqh $v14.e7,$v12.e7
    vrsql $v15.e7,$v13.e7
    vrsqh $v14.e7,vzero.e0
    /* Full 16x32 products retain precision for large gradients. */
    vmudn $v11,$v15,$v09
    vmadh $v09,$v14,$v09
    vmudn $v11,$v15,$v10
    vmadh $v10,$v14,$v10
    vmulf $v07,$v07,$v25.e0
    vmulf $v09,$v09,$v07
    vmulf $v10,$v10,$v07
    lqv $v01,0,t6
    lqv $v02,0,t7
    vadd $v01,$v01,$v10
    vsub $v02,$v02,$v09
    vge $v01,$v01,$v26.e0
    vlt $v01,$v01,$v27.e0
    vge $v02,$v02,$v26.e0
    vlt $v02,$v02,$v27.e0
    sqv $v01,0,t6
    sqv $v02,0,t7
    addiu s7,s7,-1
    bnez s7,confine_batch
    addiu s6,s6,16
    lw t3,%lo(edges)(zero)
    lw t4,%lo(edges)+4(zero)
    lw t5,%lo(edges)+8(zero)
    lw t6,%lo(edges)+12(zero)
    sw t3,%lo(urow)(zero)
    sw t4,%lo(urow)+ROW-4(zero)
    sw t5,%lo(vrow)(zero)
    sw t6,%lo(vrow)+ROW-4(zero)
    row_out s1,urow
    row_out s2,vrow
    addiu s1,s1,ROW
    addiu s2,s2,ROW
    addiu s5,s5,-1
    bnez s5,confine_row
    addiu s3,s3,ROW
    j RSPQ_Loop
    nop
