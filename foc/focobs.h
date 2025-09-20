#ifndef FOCOBS_H
#define FOCOBS_H

#include "focdef.h"

static inline void foc_obs_i_ab(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  switch (lo->e_obs) {
  case FOC_OBS_SMO: {
    DECL_SMO_PTR_RENAME(&lo->smo, smo);
    smo_exec_in(smo, in->i_ab, out->v_ab);
    in->rotor.obs_theta = smo->out.theta;
    in->rotor.obs_omega = smo->out.omega;
  } break;
  default:
    break;
  }
}

static inline void foc_obs_i_dq(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  switch (lo->e_obs) {
  case FOC_OBS_HFI: {
    DECL_HFI_PTR_RENAME(&lo->hfi, hfi);
    hfi_exec_in(hfi, in->i_dq);
    in->rotor.obs_theta = hfi->out.theta;
    in->rotor.obs_omega = hfi->out.omega;
    lo->ref_i_dq.d      = hfi->out.id;
  } break;
  default:
    break;
  }
}

static inline void foc_obs_v_dq(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  switch (lo->e_obs) {
  case FOC_OBS_HFI: {
    DECL_HFI_PTR_RENAME(&lo->hfi, hfi)
    out->v_dq.d += hfi->out.vd;
  } break;
  default:
    break;
  }
}

#endif // !FOCOBS_H
