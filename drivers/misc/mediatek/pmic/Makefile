subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6335)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6335
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6353)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6353
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6355)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6355
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6356)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6356
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6357)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6357
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6358)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6358
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6359)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6359
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6359P)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6359p
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6390)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6390

obj-$(CONFIG_MTK_EXTBUCK) += extbuck/
obj-$(CONFIG_MTK_PMIC_COMMON) += common/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6313) += mt6313/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6335) += mt6335/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6336) += mt6336/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6337) += mt6337/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6353) += mt6353/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6355) += mt6355/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6356) += mt6356/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6357) += mt6357/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6358) += mt6358/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6359) += mt6359/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6359P) += mt6359p/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6390) += mt6390/

obj-$(CONFIG_MFD_RT5081_PMU)	+= rt5081/
obj-$(CONFIG_MFD_MT6370_PMU)    += mt6370/
obj-$(CONFIG_MFD_MT6360_PMU)    += mt6360/

ifneq (,$(findstring evb,$(CONFIG_ARCH_MTK_PROJECT)))
	subdir-ccflags-y += -DMTK_EVB_PLATFORM
endif

ifeq ($(CONFIG_MTK_GAUGE_VERSION),30)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/include
# Must match the battery_* dir actually built by
# drivers/power/supply/mediatek/Makefile: struct mtk_battery (gm) has a
# single layout.  battery_bomb (CONFIG_MTK_CHARGER_BOMB=y) inserts
# bh_data/show_ag/bat_health/prev_batt_fcc/prev_batt_remaining_capacity
# (+240 bytes) before soc/ui_soc/hw_status, so code compiled against
# battery/'s header reads the wrong offsets (e.g. read_hw_ocv() saw
# hw_status.sw_ocv == 0 in mt6358_gauge.c even right after SET_SW_OCV).
ifeq ($(CONFIG_MTK_CHARGER_BOMB),y)
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery_bomb
else
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery
endif
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/misc
obj-y += mtk_battery_adc_intf.o
endif
