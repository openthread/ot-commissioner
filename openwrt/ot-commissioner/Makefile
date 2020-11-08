include $(TOPDIR)/rules.mk

PKG_NAME:=ot-commissioner
PKG_VERSION:=2020-11-08
PKG_RELEASE=$(PKG_SOURCE_VERSION)

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/openthread/ot-commissioner.git
PKG_SOURCE_VERSION:=9976ee490966ef73c3ec44bcc8c983d4d15eb61e
PKG_MAINTAINER:=IHIDCHAOS <ihidchaos@gmail.com>
include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_SOURCE_DIR = $(PKG_BUILD_DIR)

CMAKE_OPTIONS = \
	-DCMAKE_INSTALL_PREFIX=/usr


define Package/$(PKG_NAME)
	SECTION := base
	CATEGORY := Network
	TITLE := $(PKG_NAME)
	DEPENDS := +libncursesw +libreadline +libstdcpp
endef

define Package/$(PKG_NAME)/description
    a Thread commissioner for joining new Thread devices and managing Thread networks.
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/commissioner-cli $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/commissionerd.py $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/commissioner_ctl.py $(1)/usr/bin

	$(INSTALL_DIR) $(1)/usr/etc/commissioner
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/etc/commissioner/active-dataset.json $(1)/usr/etc/commissioner
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/etc/commissioner/ccm-config.json $(1)/usr/etc/commissioner
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/etc/commissioner/non-ccm-config.json $(1)/usr/etc/commissioner
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/etc/commissioner/pending-dataset.json $(1)/usr/etc/commissioner
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/etc/commissioner/README.md $(1)/usr/etc/commissioner

	$(INSTALL_DIR) $(1)/usr/etc/commissioner/credentials
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/etc/commissioner/credentials/certificate.pem $(1)/usr/etc/commissioner/credentials
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/etc/commissioner/credentials/private-key.pem $(1)/usr/etc/commissioner/credentials
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/etc/commissioner/credentials/token.hex $(1)/usr/etc/commissioner/credentials
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/etc/commissioner/credentials/trust-anchor.pem $(1)/usr/etc/commissioner/credentials

	$(INSTALL_DIR) $(1)/usr/include/commissioner
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/include/commissioner/commissioner.hpp $(1)/usr/include/commissioner
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/include/commissioner/defines.hpp $(1)/usr/include/commissioner
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/include/commissioner/error.hpp $(1)/usr/include/commissioner
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/include/commissioner/network_data.hpp $(1)/usr/include/commissioner

	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/lib/libcommissioner.a $(1)/usr/lib


endef

$(eval $(call BuildPackage,$(PKG_NAME)))
