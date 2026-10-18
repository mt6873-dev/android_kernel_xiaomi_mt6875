
ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE_pmic_throttling_dlpt.o := y
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic_wrap/$(CONFIG_MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
# Must match the battery_* dir actually built by
# drivers/power/supply/mediatek/Makefile.  struct mtk_battery (gm, built
# from battery_bomb when CONFIG_MTK_CHARGER_BOMB=y) has a different layout
# from battery/ (bh_data/show_ag/bat_health/prev_batt_fcc/
# prev_batt_remaining_capacity inserted before soc/ui_soc/hw_status), so
# compiling against battery/'s header makes this driver read/write the
# wrong offsets: read_hw_ocv() saw hw_status.sw_ocv == 0 right after the
# fuelgauged daemon's SET_SW_OCV, the daemon then got GET_HW_OCV=0 and
# fell back to its discharge ui_soc update path while charging
# (FG_DAEMON_CMD_SET_FG_BAT_INT2_HT_GAP=43 instead of ~quse/200=220), so
# the displayed charge grew ~5x faster than the coulomb counter ("fake
# fast charge").  iavg_ht/iavg_lt/pl_charger_status are also written
# through hw_status and were misplaced the same way.
ifeq ($(CONFIG_MTK_CHARGER_BOMB),y)
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery_bomb
else
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery
endif
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/misc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb_c/tcpc/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/subpmic

subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/pmic/include

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
obj-y += pmic_ipi.o
endif

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
obj-y += mt6359_gauge.o
endif

obj-$(CONFIG_MTK_PMIC_CHIP_MT6359P)	+= pmic.o pmic_common.o \
		pmic_debugfs.o \
		pmic_irq.o \
		pmic_initial_setting.o \
		pmic_api.o \
		pmic_auxadc.o \
		regulator_codegen.o \
		pmic_lp_api.o \
		pmic_efuse.o \
		pmic_throttling_dlpt.o \

obj-$(CONFIG_MTK_PMIC_CHIP_MT6359P) += pmic_oc_debug.o

