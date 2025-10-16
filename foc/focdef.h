#ifndef FOCDEF_H
#define FOCDEF_H

#include "ctl/ctl.h"
#include "filter/filter.h"
#include "obs/obs.h"
#include "trans/trans.h"
#include "util/util.h"

typedef struct {
        i32_uvw_t i32_i_uvw;
        i32       i32_v_bus;
} adc_raw_t;

typedef struct {
        f32       v_max, v_min, v_avg;
        f32_uvw_t f32_pwm_duty;
        u32_uvw_t u32_pwm_duty;
} svpwm_t;

typedef enum {
        FOC_STATE_NULL,
        FOC_STATE_CALI,
        FOC_STATE_READY,
        FOC_STATE_DISABLE,
        FOC_STATE_ENABLE,
} foc_state_e;

typedef enum {
        FOC_THETA_NULL,
        FOC_THETA_FORCE,
        FOC_THETA_SENSOR,
        FOC_THETA_SENSORLESS,
        FOC_THETA_SENSORFUSION,
} foc_theta_e;

typedef enum {
        FOC_OBS_NULL,
        FOC_OBS_SMO,
        FOC_OBS_HFI,
        FOC_OBS_LBG,
} foc_obs_e;

typedef enum {
        FOC_CALI_INIT,  // 初始化
        FOC_CALI_CW,    // 顺时针旋转校准
        FOC_CALI_CCW,   // 逆时针旋转校准
        FOC_CALI_FINISH // 校准完成
} foc_cali_e;

typedef enum {
        FOC_MODE_NULL,
        FOC_MODE_VOL,
        FOC_MODE_CUR,
        FOC_MODE_VEL,
        FOC_MODE_POS,
        FOC_MODE_PD,
        FOC_MODE_ASC,
} foc_mode_e;

typedef struct {
        u32 NULL_FUNC_PTR : 1;
} foc_fault_t;

typedef struct {
        f32 pos, ffd_vel;
        f32 vel, ffd_cur;
        f32 cur;
        f32 elec_tor;
} foc_ref_pvct_t;

typedef struct {
        f32 pos;
        f32 vel;
        f32 cur;
        f32 elec_tor, load_tor;
} foc_fdb_pvct_t;

typedef struct {
        // 电气角度
        f32 theta, comp_theta, omega;
        f32 force_theta, force_omega;
        f32 sensor_theta, sensor_comp_theta, sensor_omega;
        f32 obs_theta, obs_omega;
        f32 fusion_theta_err;
        // 机械角度
        i32 mech_cycle_cnt;
        f32 mech_theta, mech_prev_theta, mech_total_theta;
        f32 mech_omega;
} rotor_t;

typedef struct {
        f32          exec_freq;
        motor_cfg_t  motor_cfg;
        periph_cfg_t periph_cfg;
        adc_raw_t    adc_offset;
        f32          theta_offset;
        f32          ref_theta_cali_id, ref_theta_cali_omega;
        f32          sensor_theta_comp_gain, theta_comp_gain;
        pid_cfg_t    vel_cfg, pos_cfg, pd_cfg;
        u32          cur_div, vel_div, pos_div, pd_div;
} foc_cfg_t;

typedef struct {
        adc_raw_t adc_raw;
        f32       v_bus;
        rotor_t   rotor;
        f32_uvw_t f32_i_uvw;
        f32_ab_t  i_ab;
        f32_dq_t  i_dq;
} foc_in_t;

typedef struct {
        f32_dq_t  v_dq;
        f32_ab_t  v_ab;
        f32_ab_t  v_ab_sv;
        f32_uvw_t f32_v_uvw;
        svpwm_t   svpwm;
} foc_out_t;

typedef struct {
        f32 elapsed_us;
        u32 exec_cnt;
        u32 adc_cali_cnt, theta_cali_cnt, theta_cali_hold_cnt;
        f32 theta_offset_sum;

        foc_ref_pvct_t ref_pvct;
        foc_fdb_pvct_t fdb_pvct;

        f32_dq_t ref_i_dq, comp_i_dq;
        f32_dq_t ffd_v_dq;

        foc_fault_t fault;

        foc_state_e e_state;
        foc_theta_e e_theta;
        foc_mode_e  e_mode, e_last_mode;
        foc_obs_e   e_obs;
        foc_cali_e  e_cali;

        pid_ctl_t id_pid, iq_pid;
        pid_ctl_t vel_pid, pos_pid, pd_pid;

        u32 cur_div_cnt, vel_div_cnt, pos_div_cnt, pd_div_cnt;

        pll_filter_t pll;
        smo_obs_t    smo;
        hfi_obs_t    hfi;
        lbg_obs_t    lbg;
} foc_lo_t;

typedef adc_raw_t (*foc_get_adc_f)(void);
typedef f32 (*foc_get_theta_f)(void);
typedef void (*foc_set_pwm_f)(u32 pwm_full_cnt, u32_uvw_t u32_pwm_duty);
typedef void (*foc_set_drv_f)(bool enable);

typedef struct {
        foc_get_adc_f   f_get_adc;
        foc_get_theta_f f_get_theta;
        foc_set_pwm_f   f_set_pwm;
        foc_set_drv_f   f_set_drv;
} foc_ops_t;

typedef struct {
        foc_cfg_t cfg;
        foc_in_t  in;
        foc_out_t out;
        foc_lo_t  lo;
        foc_ops_t ops;
} foc_t;

#endif // !FOCDEF_H
