dtb-$(CONFIG_ARCH_ROCKCHIP) += \
	rk3368-box.dtb \
	rk3368-box-r88_808.dtb \
	rk3368-box-r88.dtb \
	rk3368-fpga.dtb \
	rk3368-p9_818.dtb \
	rk3368-tb_8846.dtb \
	rk3368-tb_mipi.dtb
dtb-$(CONFIG_ARCH_VEXPRESS) += rtsm_ve-aemv8a.dtb foundation-v8.dtb \
				fvp-base-gicv2-psci.dtb
dtb-$(CONFIG_ARCH_VEXPRESS) += juno.dtb
dtb-$(CONFIG_ARCH_XGENE) += apm-mustang.dtb

targets += dtbs

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif
targets += $(DTB_LIST)

dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files := dts/*.dtb *.dtb
