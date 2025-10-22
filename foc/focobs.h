#ifndef FOCOBS_H
#define FOCOBS_H

#include "focdef.h"

HAPI void
foc_obs_i_ab(foc_t *foc)
{
        DECL_PTRS(foc, in, out, lo);

        switch (lo->e_obs) {
                case FOC_OBS_SMO: {
                        DECL_PTR_RENAME(&lo->smo, smo);
                        smo_exec_in(smo, in->i_ab, out->v_ab);
                        in->rotor.obs_theta = smo->out.est_theta;
                        in->rotor.obs_omega = smo->out.est_omega;
                        break;
                }
                default:
                        break;
        }
}

HAPI void
foc_obs_i_dq(foc_t *foc)
{
        DECL_PTRS(foc, cfg, in, out, lo);

        switch (lo->e_obs) {
                case FOC_OBS_HFI: {
                        DECL_PTR_RENAME(&lo->hfi, hfi);
                        hfi_exec_in(hfi, in->i_dq);
                        in->rotor.obs_theta = hfi->out.est_theta;
                        in->rotor.obs_omega = hfi->out.est_omega;
                        lo->ref_i_dq.d      = hfi->out.id;
                        lo->e_mode          = (hfi->lo.e_polar_idf == HFI_POLAR_IDF_READY) ? FOC_MODE_VOL : lo->e_mode;
                        break;
                }
                case FOC_OBS_LBG: {
                        DECL_PTR_RENAME(&lo->lbg, lbg);
                        lbg_exec_in(lbg, in->rotor.theta, lo->fdb_pvct.elec_tor);
                        in->rotor.obs_theta   = lbg->out.est_theta;
                        in->rotor.obs_omega   = lbg->out.est_omega;
                        lo->fdb_pvct.load_tor = lbg->out.est_load_tor;
                        lo->comp_i_dq.q       = CPYSGN(poly_eval(cfg->motor_cfg.tor2cur,
                                                           ARRAY_SIZE(cfg->motor_cfg.tor2cur) - 1,
                                                           ABS(lo->fdb_pvct.load_tor)),
                                                 lo->fdb_pvct.load_tor);
                        break;
                }
                default:
                        break;
        }
}

HAPI void
foc_obs_v_dq(foc_t *foc)
{
        DECL_PTRS(foc, out, lo);

        switch (lo->e_obs) {
                case FOC_OBS_HFI: {
                        DECL_PTR_RENAME(&lo->hfi, hfi)
                        out->v_dq.d += hfi->out.vd;
                        break;
                }
                default:
                        break;
        }
}

#endif // !FOCOBS_H
