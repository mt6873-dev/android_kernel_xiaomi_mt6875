#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/pmic/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/power/supply/mediatek/charger
ccflags-y += -I$(srctree)/drivers/misc/mediatek/flashlight/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/spm_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb_c/tcpc/inc
subdir-ccflags-$(CONFIG_USB_MU3D_DRV) += -Werror -I$(srctree)/drivers/misc/mediatek/mu3d/drv
subdir-ccflags-$(CONFIG_USB_MTK_HDRC) += -I$(srctree)/drivers/misc/mediatek/usb20/
subdir-ccflags-$(CONFIG_USB_C_SWITCH_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/usb_c/u3_switch
subdir-ccflags-$(CONFIG_USB_C_SWITCH_U3_MUX) += -I$(srctree)/drivers/misc/mediatek/usb_c/inc
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/
# battery_bomb extends struct mtk_battery (bh_data/show_ag/bat_health/
# prev_batt_fcc/prev_batt_remaining_capacity/soc_raw).  This intf file
# dereferences get_mtk_battery()->soc/ui_soc, which are compiled into
# battery_bomb as struct mtk_battery gm, so it MUST see battery_bomb's
# layout - otherwise it reads the wrong offsets (Soc=0:0 on MT6873).
ifeq ($(CONFIG_MTK_CHARGER_BOMB),y)
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery_bomb
endif
ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery
#subdir-ccflags-$(CONFIG_CUSTOM_BATTERY_EXTERNAL_CHANNEL) += -I$(srctree)/drivers/power/supply/k6885v1_64_alpha

ifneq ($(CONFIG_FPGA_EARLY_PORTING),y)
obj-y += mtk_battery_intf.o
else
obj-y += misc.o
endif
