#include "sw/SwDevice.h"
#include "nbnxn_kernels_device.h"

typedef void (*p_nbk_func_noener)();

typedef void (*p_nbk_func_ener)();

typedef struct {
    nbnxn_pairlist_t     *nbl;
    nbnxn_atomdata_t     *nbat;
    interaction_const_t  *ic;
    rvec                       *shift_vec;
    real                       *f;
    real                       *expand_Vvdw;
    real                       *expand_Vc;
    real                       *expand_fshift;

    real *tabq_coul_F;
    real *tabq_coul_V;
    real *tabq_coul_FDV0;
} func_para_t;

__thread_local func_para_t device_func_para;

/* Analytical reaction-field kernels */
//++++++++++++++++++++++++++++++++++++
#define CALC_COUL_RF
    //===================================
    #define LJ_CUT
    #include "nbnxn_kernel_device_includes.h"
    #undef LJ_CUT

    #ifndef SW_TEST_FUNC /* in SwConfig.h */
        #define LJ_FORCE_SWITCH
        #include "nbnxn_kernel_device_includes.h"
        #undef LJ_FORCE_SWITCH

        #define LJ_POT_SWITCH
        #include "nbnxn_kernel_device_includes.h"
        #undef LJ_POT_SWITCH

        #define LJ_EWALD
            #define LJ_CUT
                #define LJ_EWALD_COMB_GEOM
                #include "nbnxn_kernel_device_includes.h"
                #undef LJ_EWALD_COMB_GEOM

                #define LJ_EWALD_COMB_LB
                #include "nbnxn_kernel_device_includes.h"
                #undef LJ_EWALD_COMB_LB
            #undef LJ_CUT
        #undef LJ_EWALD
    #endif
    //====================================
#undef CALC_COUL_RF
//++++++++++++++++++++++++++++++++++++


/* Tabulated exclusion interaction electrostatics kernels */
//++++++++++++++++++++++++++++++++++++
#define CALC_COUL_TAB
    //====================================
    #define LJ_CUT
    #include "nbnxn_kernel_device_includes.h"
    #undef LJ_CUT

    #ifndef SW_TEST_FUNC /* in SwConfig.h */
        #define LJ_FORCE_SWITCH
        #include "nbnxn_kernel_device_includes.h"
        #undef LJ_FORCE_SWITCH
    
        #define LJ_POT_SWITCH
        #include "nbnxn_kernel_device_includes.h"
        #undef LJ_POT_SWITCH

        #define LJ_EWALD
            #define LJ_CUT
                #define LJ_EWALD_COMB_GEOM
                #include "nbnxn_kernel_device_includes.h"
                #undef LJ_EWALD_COMB_GEOM

                #define LJ_EWALD_COMB_LB
                #include "nbnxn_kernel_device_includes.h"
                #undef LJ_EWALD_COMB_LB
            #undef LJ_CUT
        #undef LJ_EWALD
    #endif
    //====================================



    /* Twin-range cut-off kernels */
    //====================================
    #ifndef SW_TEST_FUNC /* in SwConfig.h */

    #define VDW_CUTOFF_CHECK
        #define LJ_CUT
        #include "nbnxn_kernel_device_includes.h"
        #undef LJ_CUT

        #define LJ_FORCE_SWITCH
        #include "nbnxn_kernel_device_includes.h"
        #undef LJ_FORCE_SWITCH

        #define LJ_POT_SWITCH
        #include "nbnxn_kernel_device_includes.h"
        #undef LJ_POT_SWITCH

        #define LJ_EWALD
            #define LJ_CUT
                #define LJ_EWALD_COMB_GEOM
                #include "nbnxn_kernel_device_includes.h"
                #undef LJ_EWALD_COMB_GEOM

                #define LJ_EWALD_COMB_LB
                #include "nbnxn_kernel_device_includes.h"
                #undef LJ_EWALD_COMB_LB
            #undef LJ_CUT
        #undef LJ_EWALD
    #undef VDW_CUTOFF_CHECK

    #endif
    //====================================
#undef CALC_COUL_TAB
//++++++++++++++++++++++++++++++++++++

enum {
    coultRF, coultTAB, coultTAB_TWIN, coultNR
};

enum {
    vdwtCUT, vdwtFSWITCH, vdwtPSWITCH, vdwtEWALDGEOM, vdwtEWALDLB, vdwtNR
};

__thread_local p_nbk_func_noener p_nbk_c_noener_device[coultNR][vdwtNR] =
{
#ifdef SW_TEST_FUNC /* in SwConfig.h */
    { nbnxn_kernel_ElecRF_VdwLJ_F_device,           NULL, NULL, NULL, NULL           },
    { nbnxn_kernel_ElecQSTab_VdwLJ_F_device,        NULL, NULL, NULL, NULL           },
    { NULL, NULL, NULL, NULL, NULL  }
#else
    { nbnxn_kernel_ElecRF_VdwLJ_F_device,           nbnxn_kernel_ElecRF_VdwLJFsw_F_device,           nbnxn_kernel_ElecRF_VdwLJPsw_F_device,           nbnxn_kernel_ElecRF_VdwLJEwCombGeom_F_device,           nbnxn_kernel_ElecRF_VdwLJEwCombLB_F_device           },
    { nbnxn_kernel_ElecQSTab_VdwLJ_F_device,        nbnxn_kernel_ElecQSTab_VdwLJFsw_F_device,        nbnxn_kernel_ElecQSTab_VdwLJPsw_F_device,        nbnxn_kernel_ElecQSTab_VdwLJEwCombGeom_F_device,        nbnxn_kernel_ElecQSTab_VdwLJEwCombLB_F_device        },
    { nbnxn_kernel_ElecQSTabTwinCut_VdwLJ_F_device, nbnxn_kernel_ElecQSTabTwinCut_VdwLJFsw_F_device, nbnxn_kernel_ElecQSTabTwinCut_VdwLJPsw_F_device, nbnxn_kernel_ElecQSTabTwinCut_VdwLJEwCombGeom_F_device, nbnxn_kernel_ElecQSTabTwinCut_VdwLJEwCombLB_F_device }
#endif
};

__thread_local p_nbk_func_ener p_nbk_c_ener_device[coultNR][vdwtNR] =
{
#ifdef SW_TEST_FUNC /* in SwConfig.h */
    { nbnxn_kernel_ElecRF_VdwLJ_VF_device,           NULL, NULL, NULL, NULL           },
    { nbnxn_kernel_ElecQSTab_VdwLJ_VF_device,        NULL, NULL, NULL, NULL           },
    { NULL, NULL, NULL, NULL, NULL  }
#else
    { nbnxn_kernel_ElecRF_VdwLJ_VF_device,           nbnxn_kernel_ElecRF_VdwLJFsw_VF_device,           nbnxn_kernel_ElecRF_VdwLJPsw_VF_device,           nbnxn_kernel_ElecRF_VdwLJEwCombGeom_VF_device,           nbnxn_kernel_ElecRF_VdwLJEwCombLB_VF_device            },
    { nbnxn_kernel_ElecQSTab_VdwLJ_VF_device,        nbnxn_kernel_ElecQSTab_VdwLJFsw_VF_device,        nbnxn_kernel_ElecQSTab_VdwLJPsw_VF_device,        nbnxn_kernel_ElecQSTab_VdwLJEwCombGeom_VF_device,        nbnxn_kernel_ElecQSTab_VdwLJEwCombLB_VF_device         },
    { nbnxn_kernel_ElecQSTabTwinCut_VdwLJ_VF_device, nbnxn_kernel_ElecQSTabTwinCut_VdwLJFsw_VF_device, nbnxn_kernel_ElecQSTabTwinCut_VdwLJPsw_VF_device, nbnxn_kernel_ElecQSTabTwinCut_VdwLJEwCombGeom_VF_device, nbnxn_kernel_ElecQSTabTwinCut_VdwLJEwCombLB_VF_device  }
#endif
};
#ifdef SW_ENERGRP /* in SwConfig */

__thread_local p_nbk_func_ener p_nbk_c_energrp_device[coultNR][vdwtNR] =
{
#ifdef SW_TEST_FUNC /* in SwConfig.h */
    { nbnxn_kernel_ElecRF_VdwLJ_VgrpF_device,           NULL, NULL, NULL, NULL           },
    { nbnxn_kernel_ElecQSTab_VdwLJ_VgrpF_device,        NULL, NULL, NULL, NULL           },
    { NULL, NULL, NULL, NULL, NULL  }
#else
    { nbnxn_kernel_ElecRF_VdwLJ_VgrpF_device,           nbnxn_kernel_ElecRF_VdwLJFsw_VgrpF_device,           nbnxn_kernel_ElecRF_VdwLJPsw_VgrpF_device,           nbnxn_kernel_ElecRF_VdwLJEwCombGeom_VgrpF_device,           nbnxn_kernel_ElecRF_VdwLJEwCombLB_VgrpF_device           },
    { nbnxn_kernel_ElecQSTab_VdwLJ_VgrpF_device,        nbnxn_kernel_ElecQSTab_VdwLJFsw_VgrpF_device,        nbnxn_kernel_ElecQSTab_VdwLJPsw_VgrpF_device,        nbnxn_kernel_ElecQSTab_VdwLJEwCombGeom_VgrpF_device,        nbnxn_kernel_ElecQSTab_VdwLJEwCombLB_VgrpF_device        },
    { nbnxn_kernel_ElecQSTabTwinCut_VdwLJ_VgrpF_device, nbnxn_kernel_ElecQSTabTwinCut_VdwLJFsw_VgrpF_device, nbnxn_kernel_ElecQSTabTwinCut_VdwLJPsw_VgrpF_device, nbnxn_kernel_ElecQSTabTwinCut_VdwLJEwCombGeom_VgrpF_device, nbnxn_kernel_ElecQSTabTwinCut_VdwLJEwCombLB_VgrpF_device }
#endif
};

#endif


void kaCHI_func(int n)
{
    TLOG("kaCHI Func called %d.\n", n);
}


void device_run()
{
#ifdef DEBUG_SDLB
    TLOG("kaCHI 0.\n");
#endif
    int func_type = device_in_param[FUNC_TYPE];
    int func_i    = device_in_param[FUNC_I];
    int func_j    = device_in_param[FUNC_J];
    device_func_para     = *((func_para_t*)device_in_param[FUNC_PARAM_PTR]);
#ifdef SW_DEVICE_LOG /* in SwConfig */
    if(((device_notice_counter + device_param.host_rank - 1) % 64) == 0)
        OLOG("FuncType =%d, I =%d, J =%d\n", func_type, func_i, func_j);
#endif
    if(func_type > 1 || func_i > 1 || func_j > 0)
    {
        OLOG("UNKNOWN FUNC: FuncType =%d, I =%d, J =%d\n", func_type, func_i, func_j);
        return;
    }
#define SW_PRINT_PARASIZE
#undef SW_PRINT_PARASIZE
#ifdef SW_PRINT_PARASIZE
    {
        int natoms = device_func_para.nbat->natoms;
        int fstride = device_func_para.nbat->fstride;
        int sizeof_f = natoms*fstride;
        int sizeof_fshift = SHIFTS*DIM;
        if(((device_notice_counter + device_param.host_rank - 1) % 64) == 0)
            OLOG("natoms =%d, fstride =%d, sizeof_f =%d, sizeof_fshift =%d\n", natoms, fstride, sizeof_f, sizeof_fshift);
    }
#endif
#ifdef DEBUG_SDLB
    TLOG("kaCHI 0.1.\n");
    kaCHI_func(0);
    TLOG("kaCHI sizeof(nbnxn_pairlist_t) =%d\n", sizeof(nbnxn_pairlist_t));
    //wait_host(device_core_id);
#endif
    if(func_type == FUNC_NO_ENER)
    {
        p_nbk_c_noener_device[func_i][func_j]();
    }
    else if(func_type == FUNC_ENER)
    {
        p_nbk_c_ener_device[func_i][func_j]();
    }
#ifdef DEBUG_SDLB
    kaCHI_func(1);
    TLOG("kaCHI Final.\n");
#endif
}