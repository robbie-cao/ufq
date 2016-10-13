include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME      := ufqd
PKG_VERSION   := 0.0.1
PKG_RELEASE   := 1
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

define Package/ufqd
    SECTION:=utils
	CATEGORY:=Utilities
	DEFAULT:=y
	TITLE:=File Queue Daemon
	DEPENDS:=+ubusd +ubus +ubox +libubus +libubox +libblobmsg-json
endef

TARGET_CFLAGS += -Wall
EXTRA_LDFLAGS += -lubus -lubox -lblobmsg_json

define Build/Prepare
	$(Build/Prepare/Default)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/ufqd/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ufqd $(1)/usr/bin
endef

$(eval $(call BuildPackage,ufqd))
