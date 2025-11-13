/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 1999 - 2025 Intel Corporation. */

/* ethtool support for ixgbe */
#include "ixgbevf.h"

#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/vmalloc.h>
#ifdef SIOCETHTOOL
#include <asm/uaccess.h>

#ifndef ETH_GSTRING_LEN
#define ETH_GSTRING_LEN 32
#endif

#define IXGBE_ALL_RAR_ENTRIES 16

#ifdef ETHTOOL_OPS_COMPAT
#include "kcompat_ethtool.c"
#endif
#ifdef ETHTOOL_GSTATS

enum {NETDEV_STATS, IXGBEVF_STATS};

struct ixgbe_stats {
	char stat_string[ETH_GSTRING_LEN];
	int type;
	int sizeof_stat;
	int stat_offset;
};

#define IXGBEVF_STAT(_name, _stat) { \
	.stat_string = _name, \
	.type = IXGBEVF_STATS, \
	.sizeof_stat = sizeof_field(struct ixgbevf_adapter, _stat), \
	.stat_offset = offsetof(struct ixgbevf_adapter, _stat) \
}

#define IXGBEVF_NETDEV_STAT(_net_stat) { \
	.stat_string = #_net_stat, \
	.type = NETDEV_STATS, \
	.sizeof_stat = sizeof_field(struct net_device_stats, _net_stat), \
	.stat_offset = offsetof(struct net_device_stats, _net_stat) \
}

static struct ixgbe_stats ixgbe_gstrings_stats[] = {
	IXGBEVF_NETDEV_STAT(rx_packets),
	IXGBEVF_NETDEV_STAT(tx_packets),
	IXGBEVF_NETDEV_STAT(rx_bytes),
	IXGBEVF_NETDEV_STAT(tx_bytes),
	IXGBEVF_STAT("tx_busy", tx_busy),
	IXGBEVF_STAT("tx_restart_queue", restart_queue),
	IXGBEVF_STAT("tx_timeout_count", tx_timeout_count),
	IXGBEVF_NETDEV_STAT(multicast),
	IXGBEVF_STAT("rx_csum_offload_errors", hw_csum_rx_error),
	IXGBEVF_STAT("alloc_rx_page", alloc_rx_page),
	IXGBEVF_STAT("alloc_rx_page_failed", alloc_rx_page_failed),
	IXGBEVF_STAT("alloc_rx_buff_failed", alloc_rx_buff_failed),
};

#define IXGBEVF_QUEUE_STATS_LEN ( \
	(((struct ixgbevf_adapter *)netdev_priv(netdev))->num_tx_queues + \
	 ((struct ixgbevf_adapter *)netdev_priv(netdev))->num_xdp_queues + \
	 ((struct ixgbevf_adapter *)netdev_priv(netdev))->num_rx_queues) * \
	 (sizeof(struct ixgbevf_stats) / sizeof(u64)))
#define IXGBEVF_GLOBAL_STATS_LEN	ARRAY_SIZE(ixgbe_gstrings_stats)

#define IXGBEVF_STATS_LEN (IXGBEVF_GLOBAL_STATS_LEN + IXGBEVF_QUEUE_STATS_LEN)
#endif /* ETHTOOL_GSTATS */

#ifdef ETHTOOL_TEST
static const char ixgbe_gstrings_test[][ETH_GSTRING_LEN] = {
	"Register test  (offline)",
	"Link test   (on/offline)"
};
#define IXGBEVF_TEST_LEN (sizeof(ixgbe_gstrings_test) / ETH_GSTRING_LEN)
#endif /* ETHTOOL_TEST */

#if defined(HAVE_ETHTOOL_GET_SSET_COUNT) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
static const char ixgbevf_priv_flags_strings[][ETH_GSTRING_LEN] = {
#define IXGBEVF_PRIV_FLAGS_LEGACY_RX	BIT(0)
	"legacy-rx",
};

#define IXGBEVF_PRIV_FLAGS_STR_LEN ARRAY_SIZE(ixgbevf_priv_flags_strings)
#endif /* HAVE_ETHTOOL_GET_SSET_COUNT && HAVE_SWIOTLB_SKIP_CPU_SYNC */
#ifdef HAVE_ETHTOOL_CONVERT_U32_AND_LINK_MODE
/**
 * ixgbevf_get_link_ksettings - Retrieve link settings for a network device
 * @netdev: Pointer to the network device structure
 * @cmd: Pointer to the ethtool link settings structure to be filled
 *
 * This function populates the provided ethtool link settings structure with
 * the current link settings of the specified network device. It sets the
 * supported link modes, autonegotiation status, and port type. Additionally,
 * it determines the current link speed and duplex mode based on the adapter's
 * link status and speed.
 *
 * The function supports conditional compilation to accommodate different kernel
 * versions, which may require different methods for setting link modes and
 * speeds.
 *
 * Return: 0 on success.
 */
static int ixgbevf_get_link_ksettings(struct net_device *netdev,
				      struct ethtool_link_ksettings *cmd)
#else
/**
 * ixgbevf_get_settings - Retrieve link settings for a network device
 * @netdev: Pointer to the network device structure
 * @ecmd: Pointer to the ethtool command structure to be filled
 *
 * This function populates the provided ethtool command structure with the
 * current link settings of the specified network device. It sets the supported
 * link modes, autonegotiation status, and transceiver type. Additionally, it
 * determines the current link speed and duplex mode based on the adapter's
 * link status and speed.
 *
 * The function supports conditional compilation to accommodate different kernel
 * versions, which may require different methods for setting link modes and
 * speeds.
 *
 * Return: 0 on success.
 */
static int ixgbevf_get_settings(struct net_device *netdev,
				struct ethtool_cmd *ecmd)
#endif
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
	__u32 speed = SPEED_UNKNOWN;

#ifdef HAVE_ETHTOOL_CONVERT_U32_AND_LINK_MODE
	ethtool_link_ksettings_zero_link_mode(cmd, supported);

	if (adapter->link_up) {
		switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_10GB_FULL:
			speed = SPEED_10000;
			ethtool_link_ksettings_add_link_mode(cmd, supported,
							     10000baseT_Full);
			break;
		case IXGBE_LINK_SPEED_5GB_FULL:
			speed = SPEED_5000;
			ethtool_link_ksettings_add_link_mode(cmd, supported,
							     5000baseT_Full);
			break;
#ifdef SUPPORTED_2500baseX_Full
		case IXGBE_LINK_SPEED_2_5GB_FULL:
			speed = SPEED_2500;
			ethtool_link_ksettings_add_link_mode(cmd, supported,
							     2500baseT_Full);
			break;
#endif /* SUPPORTED_2500baseX_Full */
		case IXGBE_LINK_SPEED_1GB_FULL:
			speed = SPEED_1000;
			ethtool_link_ksettings_add_link_mode(cmd, supported,
							     1000baseT_Full);
			break;
		case IXGBE_LINK_SPEED_100_FULL:
			speed = SPEED_100;
			ethtool_link_ksettings_add_link_mode(cmd, supported,
							     100baseT_Full);
			break;
		case IXGBE_LINK_SPEED_10_FULL:
			speed = SPEED_10;
			ethtool_link_ksettings_add_link_mode(cmd, supported,
							     10baseT_Full);
			break;
		default:
			break;
		}
		cmd->base.speed = speed;
		cmd->base.duplex = DUPLEX_FULL;
	} else {
		cmd->base.speed = SPEED_UNKNOWN;
		cmd->base.duplex = DUPLEX_UNKNOWN;
	}

	cmd->base.autoneg = AUTONEG_DISABLE;
	cmd->base.port = -1;
#else
	ecmd->supported = 0;

	if (adapter->link_up) {
		switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_10GB_FULL:
			speed = SPEED_10000;
			ecmd->supported |= SUPPORTED_10000baseT_Full;
			break;
		case IXGBE_LINK_SPEED_5GB_FULL:
			speed = SPEED_5000;
			ecmd->supported |= SUPPORTED_5000baseT_Full;
			break;
#ifdef SUPPORTED_2500baseX_Full
		case IXGBE_LINK_SPEED_2_5GB_FULL:
			speed = SPEED_2500;
			ecmd->supported |= SUPPORTED_2500baseT_Full;
			break;
#endif /* SUPPORTED_2500baseX_Full */
		case IXGBE_LINK_SPEED_1GB_FULL:
			speed = SPEED_1000;
			ecmd->supported |= SUPPORTED_1000baseT_Full;
			break;
		case IXGBE_LINK_SPEED_100_FULL:
			speed = SPEED_100;
			ecmd->supported |= SUPPORTED_100baseT_Full;
			break;
		case IXGBE_LINK_SPEED_10_FULL:
			speed = SPEED_10;
			ecmd->supported |= SUPPORTED_10baseT_Full;
			break;
		default:
			break;
		}

		ethtool_cmd_speed_set(ecmd, speed);
		ecmd->duplex = DUPLEX_FULL;
	} else {
		ethtool_cmd_speed_set(ecmd, SPEED_UNKNOWN);
		ecmd->duplex = DUPLEX_UNKNOWN;
	}

	ecmd->autoneg = AUTONEG_DISABLE;
	ecmd->transceiver = XCVR_DUMMY1;
	ecmd->port = -1;
#endif

	return 0;
}

#ifdef HAVE_ETHTOOL_CONVERT_U32_AND_LINK_MODE
/**
 * ixgbevf_set_link_ksettings - Attempt to set link settings on the network device
 * @netdev: Unused pointer to the network device structure
 * @cmd: Unused pointer to the ethtool link settings structure
 *
 * This function is intended to set link settings on the specified network device.
 * However, the operation is not supported for this driver, and the function
 * always returns -EOPNOTSUPP. The parameters are marked as `__always_unused`,
 * indicating that they are not utilized in this function.
 *
 * Return: -EINVAL, indicating that the operation is not supported.
 */
static int ixgbevf_set_link_ksettings(struct net_device __always_unused *netdev,
		       const struct ethtool_link_ksettings __always_unused *cmd)
#else
/**
 * ixgbevf_set_settings - Attempt to set ethtool settings on the network device
 * @netdev: Unused pointer to the network device structure
 * @ecmd: Unused pointer to the ethtool command structure
 *
 * This function is intended to set ethtool settings on the specified network
 * device. However, the operation is not supported for this driver, and the
 * function always returns -EOPNOTSUPP. The parameters are marked as
 * `__always_unused`, indicating that they are not utilized in this function.
 *
 * Return: -EINVAL, indicating that the operation is not supported.
 */
static int ixgbevf_set_settings(struct net_device __always_unused *netdev,
				struct ethtool_cmd __always_unused *ecmd)
#endif
{
	return -EINVAL;
}

#ifndef HAVE_NDO_SET_FEATURES
/**
 * ixgbevf_get_rx_csum - Retrieve the RX checksum offload setting
 * @netdev: Pointer to the network device structure
 *
 * This function returns the current setting for receive (RX) checksum offloading
 * for the specified network device. RX checksum offloading allows the network
 * device to handle checksum calculations for incoming packets, reducing CPU
 * load. The function provides the status of this feature, indicating whether
 * it is enabled or disabled.
 *
 * Return: A 32-bit unsigned integer representing the RX checksum offload status.
 */
static u32 ixgbevf_get_rx_csum(struct net_device *netdev)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
	return (adapter->flags & IXGBE_FLAG_RX_CSUM_ENABLED);
}

/**
 * ixgbevf_set_rx_csum - Enable or disable RX checksum offloading
 * @netdev: Pointer to the network device structure
 * @data: Flag indicating whether to enable (non-zero) or disable (zero) RX checksum offloading
 *
 * This function enables or disables receive (RX) checksum offloading for the
 * specified network device based on the provided @data flag. RX checksum
 * offloading allows the network device to handle checksum calculations for
 * incoming packets, reducing CPU load. If the network interface is running,
 * the function reinitializes the adapter to apply the changes. If the interface
 * is not running, it performs a reset to ensure the new settings take effect.
 *
 * Return: 0 on success.
 */
static int ixgbevf_set_rx_csum(struct net_device *netdev, u32 data)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
	if (data)
		adapter->flags |= IXGBE_FLAG_RX_CSUM_ENABLED;
	else
		adapter->flags &= ~IXGBE_FLAG_RX_CSUM_ENABLED;

	if (netif_running(netdev)) {
		if (!adapter->dev_closed) {
			ixgbevf_reinit_locked(adapter);
		}
	} else {
		ixgbevf_reset(adapter);
	}

	return 0;
}

/**
 * ixgbevf_get_tx_csum - Retrieve the TX checksum offload setting
 * @netdev: Pointer to the network device structure
 *
 * This function checks whether transmit (TX) checksum offloading is enabled
 * for the specified network device. TX checksum offloading allows the network
 * device to handle checksum calculations for outgoing packets, reducing CPU
 * load. The function returns a non-zero value if TX checksum offloading is
 * enabled, and zero if it is not.
 *
 * Return: Non-zero if TX checksum offloading is enabled, zero otherwise.
 */
static u32 ixgbevf_get_tx_csum(struct net_device *netdev)
{
	return (netdev->features & NETIF_F_IP_CSUM) != 0;
}

/**
 * ixgbevf_set_tx_csum - Enable or disable TX checksum offloading
 * @netdev: Pointer to the network device structure
 * @data: Flag indicating whether to enable (non-zero) or disable (zero) TX checksum offloading
 *
 * This function enables or disables transmit (TX) checksum offloading for the
 * specified network device based on the provided @data flag. TX checksum
 * offloading allows the network device to handle checksum calculations for
 * outgoing packets, reducing CPU load. The function updates the device features
 * to include or exclude checksum offloading capabilities for both IPv4 and IPv6,
 * depending on the kernel configuration.
 *
 * Return: 0 on success.
 */
static int ixgbevf_set_tx_csum(struct net_device *netdev, u32 data)
{
	if (data)
#ifdef NETIF_F_IPV6_CSUM
		netdev->features |= (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM);
	else
		netdev->features &= ~(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM);
#else
		netdev->features |= NETIF_F_IP_CSUM;
	else
		netdev->features &= ~NETIF_F_IP_CSUM;
#endif

	return 0;
}
#endif

#ifndef HAVE_NDO_SET_FEATURES
#ifdef NETIF_F_TSO
/**
 * ixgbevf_set_tso - Enable or disable TCP Segmentation Offload (TSO)
 * @netdev: Pointer to the network device structure
 * @data: Flag indicating whether to enable (non-zero) or disable (zero) TSO
 *
 * This function enables or disables TCP Segmentation Offload (TSO) for the
 * specified network device based on the provided @data flag. TSO allows the
 * network device to handle the segmentation of large TCP packets, reducing CPU
 * load. If TSO is disabled, the function stops all transmit queues, updates
 * the device features to remove TSO capabilities, and then restarts the queues.
 * The function also handles disabling TSO for VLAN devices if they are present
 * and VLAN features are not supported by the netdev.
 *
 * The function supports conditional compilation to accommodate different kernel
 * versions and feature sets.
 *
 * Return: 0 on success.
 */
static int ixgbevf_set_tso(struct net_device *netdev, u32 data)
{
#ifndef HAVE_NETDEV_VLAN_FEATURES
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
#endif /* HAVE_NETDEV_VLAN_FEATURES */
	if (data) {
		netdev->features |= NETIF_F_TSO;
#ifdef NETIF_F_TSO6
		netdev->features |= NETIF_F_TSO6;
#endif
	} else {
		netif_tx_stop_all_queues(netdev);
		netdev->features &= ~NETIF_F_TSO;
#ifdef NETIF_F_TSO6
		netdev->features &= ~NETIF_F_TSO6;
#endif
#ifndef HAVE_NETDEV_VLAN_FEATURES
#ifdef NETIF_F_HW_VLAN_TX
		/* disable TSO on all VLANs if they're present */
		if (adapter->vlgrp) {
			int i;
			struct net_device *v_netdev;
			for (i = 0; i < VLAN_GROUP_ARRAY_LEN; i++) {
				v_netdev =
				       vlan_group_get_device(adapter->vlgrp, i);
				if (v_netdev) {
					v_netdev->features &= ~NETIF_F_TSO;
#ifdef NETIF_F_TSO6
					v_netdev->features &= ~NETIF_F_TSO6;
#endif
					vlan_group_set_device(adapter->vlgrp, i,
					                      v_netdev);
				}
			}
		}
#endif
#endif /* HAVE_NETDEV_VLAN_FEATURES */
		netif_tx_start_all_queues(netdev);
	}
	return 0;
}
#endif /* NETIF_F_TSO */
#endif

/**
 * ixgbevf_get_msglevel - Retrieve the message logging level for a network device
 * @netdev: Pointer to the network device structure
 *
 * This function returns the current message logging level for the specified
 * network device. The message level determines the verbosity of the logging
 * output for the device, allowing users to control the amount of information
 * logged by the driver. This is typically used for debugging and monitoring
 * purposes.
 *
 * Return: The current message logging level as a 32-bit unsigned integer.
 */
static u32 ixgbevf_get_msglevel(struct net_device *netdev)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
	return adapter->msg_enable;
}

/**
 * ixgbevf_set_msglevel - Set the message logging level for a network device
 * @netdev: Pointer to the network device structure
 * @data: The new message logging level to be set
 *
 * This function sets the message logging level for the specified network
 * device. The message level determines the verbosity of the logging output
 * for the device, allowing users to control the amount of information logged
 * by the driver. The new logging level is specified by the @data parameter
 * and is stored in the adapter's msg_enable field.
 */
static void ixgbevf_set_msglevel(struct net_device *netdev, u32 data)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
	adapter->msg_enable = data;
}

/**
 * ixgbevf_get_regs_len - Get the length of the device registers
 * @netdev: Unused pointer to the network device structure
 *
 * This function returns the length of the device registers for the ixgbevf
 * driver. The length indicates the size of the register space that can be
 * accessed for diagnostic or debugging purposes. The @netdev parameter is
 * marked as `__always_unused`, indicating that it is not utilized in this
 * function.
 *
 * Return: The length of the device registers as an integer.
 */
static int ixgbevf_get_regs_len(struct net_device __always_unused *netdev)
{
#define IXGBE_REGS_LEN  45
	return IXGBE_REGS_LEN * sizeof(u32);
}

#define IXGBE_GET_STAT(_A_, _R_) _A_->stats._R_

/**
 * ixgbevf_get_regs - Retrieve device registers for a network device
 * @netdev: Pointer to the network device structure
 * @regs: Pointer to the ethtool_regs structure to be filled with register data
 * @p: Pointer to a buffer where the register data will be stored
 *
 * This function populates the provided ethtool_regs structure and buffer with
 * the current register values of the specified network device. This information
 * is typically used for diagnostic and debugging purposes, allowing users to
 * inspect the state of the device's hardware registers.
 */
static void ixgbevf_get_regs(struct net_device *netdev, struct ethtool_regs *regs,
                           void *p)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	u32 *regs_buff = p;
	u8 i;

	memset(p, 0, IXGBE_REGS_LEN * sizeof(u32));

	/* generate a number suitable for ethtool's register version */
	regs->version = (1u << 24) | hw->revision_id << 16 | hw->device_id;

	/* IXGBE_VFCTRL is a Write Only register, so just return 0 */
	regs_buff[0] = 0x0;

	/* General Registers */
	regs_buff[1] = IXGBE_READ_REG(hw, IXGBE_VFSTATUS);
	regs_buff[2] = IXGBE_READ_REG(hw, IXGBE_VFLINKS);
	regs_buff[3] = IXGBE_READ_REG(hw, IXGBE_VFRXMEMWRAP);
	regs_buff[4] = IXGBE_READ_REG(hw, IXGBE_VFFRTIMER);

	/* Interrupt */
	/* don't read EICR because it can clear interrupt causes, instead
	 * read EICS which is a shadow but doesn't clear EICR */
	regs_buff[5] = IXGBE_READ_REG(hw, IXGBE_VTEICS);
	regs_buff[6] = IXGBE_READ_REG(hw, IXGBE_VTEICS);
	regs_buff[7] = IXGBE_READ_REG(hw, IXGBE_VTEIMS);
	regs_buff[8] = IXGBE_READ_REG(hw, IXGBE_VTEIMC);
	regs_buff[9] = IXGBE_READ_REG(hw, IXGBE_VTEIAC);
	regs_buff[10] = IXGBE_READ_REG(hw, IXGBE_VTEIAM);
	regs_buff[11] = IXGBE_READ_REG(hw, IXGBE_VTEITR(0));
	regs_buff[12] = IXGBE_READ_REG(hw, IXGBE_VTIVAR(0));
	regs_buff[13] = IXGBE_READ_REG(hw, IXGBE_VTIVAR_MISC);

	/* Receive DMA */
	for (i = 0; i < 2; i++)
		regs_buff[14 + i] = IXGBE_READ_REG(hw, IXGBE_VFRDBAL(i));
	for (i = 0; i < 2; i++)
		regs_buff[16 + i] = IXGBE_READ_REG(hw, IXGBE_VFRDBAH(i));
	for (i = 0; i < 2; i++)
		regs_buff[18 + i] = IXGBE_READ_REG(hw, IXGBE_VFRDLEN(i));
	for (i = 0; i < 2; i++)
		regs_buff[20 + i] = IXGBE_READ_REG(hw, IXGBE_VFRDH(i));
	for (i = 0; i < 2; i++)
		regs_buff[22 + i] = IXGBE_READ_REG(hw, IXGBE_VFRDT(i));
	for (i = 0; i < 2; i++)
		regs_buff[24 + i] = IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(i));
	for (i = 0; i < 2; i++)
		regs_buff[26 + i] = IXGBE_READ_REG(hw, IXGBE_VFSRRCTL(i));

	/* Receive */
	regs_buff[28] = IXGBE_READ_REG(hw, IXGBE_VFPSRTYPE);

	/* Transmit */
	for (i = 0; i < 2; i++)
		regs_buff[29 + i] = IXGBE_READ_REG(hw, IXGBE_VFTDBAL(i));
	for (i = 0; i < 2; i++)
		regs_buff[31 + i] = IXGBE_READ_REG(hw, IXGBE_VFTDBAH(i));
	for (i = 0; i < 2; i++)
		regs_buff[33 + i] = IXGBE_READ_REG(hw, IXGBE_VFTDLEN(i));
	for (i = 0; i < 2; i++)
		regs_buff[35 + i] = IXGBE_READ_REG(hw, IXGBE_VFTDH(i));
	for (i = 0; i < 2; i++)
		regs_buff[37 + i] = IXGBE_READ_REG(hw, IXGBE_VFTDT(i));
	for (i = 0; i < 2; i++)
		regs_buff[39 + i] = IXGBE_READ_REG(hw, IXGBE_VFTXDCTL(i));
	for (i = 0; i < 2; i++)
		regs_buff[41 + i] = IXGBE_READ_REG(hw, IXGBE_VFTDWBAL(i));
	for (i = 0; i < 2; i++)
		regs_buff[43 + i] = IXGBE_READ_REG(hw, IXGBE_VFTDWBAH(i));

}

/**
 * ixgbevf_get_eeprom - Retrieve EEPROM data from the network device
 * @netdev: Unused pointer to the network device structure
 * @eeprom: Unused pointer to the ethtool EEPROM structure
 * @bytes: Unused pointer to the buffer for storing EEPROM data
 *
 * This function is intended to retrieve EEPROM data from the network device.
 * However, the parameters are marked as `__always_unused`, indicating that
 * this function does not currently utilize them. This may be a placeholder
 * or a stub function for future implementation.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int ixgbevf_get_eeprom(struct net_device __always_unused *netdev,
			      struct ethtool_eeprom __always_unused *eeprom,
			      u8 __always_unused *bytes)
{
	return -EOPNOTSUPP;
}

/**
 * ixgbevf_set_eeprom - Attempt to set EEPROM data on the network device
 * @netdev: Unused pointer to the network device structure
 * @eeprom: Unused pointer to the ethtool EEPROM structure
 * @bytes: Unused pointer to the buffer containing EEPROM data
 *
 * This function is intended to set EEPROM data on the specified network device.
 * However, the operation is not supported for this driver, and the function
 * always returns -EOPNOTSUPP. The parameters are marked as `__always_unused`,
 * indicating that they are not utilized in this function.
 *
 * Return: -EOPNOTSUPP, indicating that the operation is not supported.
 */
static int ixgbevf_set_eeprom(struct net_device __always_unused *netdev,
			      struct ethtool_eeprom __always_unused *eeprom,
			      u8 __always_unused *bytes)
{
	return -EOPNOTSUPP;
}

/**
 * ixgbevf_get_drvinfo - Retrieve driver information for a network device
 * @netdev: Pointer to the network device structure
 * @drvinfo: Pointer to the ethtool driver info structure to be filled
 *
 * This function populates the provided ethtool driver info structure with
 * information about the driver and the network device. This typically includes
 * details such as the driver version, firmware version, and bus information.
 * The information is used by ethtool to display driver-related data to the user.
 */
static void ixgbevf_get_drvinfo(struct net_device *netdev,
				struct ethtool_drvinfo *drvinfo)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);

	strscpy(drvinfo->driver, ixgbevf_driver_name, sizeof(drvinfo->driver));
	strscpy(drvinfo->version, ixgbevf_driver_version,
		sizeof(drvinfo->version));
	strscpy(drvinfo->bus_info, pci_name(adapter->pdev),
		sizeof(drvinfo->bus_info));
#if defined(HAVE_ETHTOOL_GET_SSET_COUNT) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)

	drvinfo->n_priv_flags = IXGBEVF_PRIV_FLAGS_STR_LEN;
#endif /* HAVE_ETHTOOL_GET_SSET_COUNT && HAVE_SWIOTLB_SKIP_CPU_SYNC */
}

#ifdef HAVE_ETHTOOL_EXTENDED_RINGPARAMS
/**
 * ixgbevf_get_ringparam - Retrieve ring parameters for a network device
 * @netdev: Pointer to the network device structure
 * @ring: Pointer to the ethtool_ringparam structure to be filled
 * @ker: Unused pointer to the kernel ethtool ringparam structure
 * @extack: Unused pointer to netlink extended ACK structure
 *
 * This function populates the provided ethtool_ringparam structure with the
 * current ring parameters of the specified network device. Ring parameters
 * include details such as the number of transmit and receive descriptors.
 * The @ker and @extack parameters are marked as `__always_unused`, indicating
 * that they are not utilized in this function.
 */
static void
ixgbevf_get_ringparam(struct net_device *netdev,
		      struct ethtool_ringparam *ring,
		      struct kernel_ethtool_ringparam __always_unused *ker,
		      struct netlink_ext_ack __always_unused *extack)
#else
static void ixgbevf_get_ringparam(struct net_device *netdev,
				  struct ethtool_ringparam *ring)
#endif
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);

	ring->rx_max_pending = IXGBEVF_MAX_NUM_DESCRIPTORS;
	ring->tx_max_pending = IXGBEVF_MAX_NUM_DESCRIPTORS;
	ring->rx_pending = adapter->rx_ring_count;
	ring->tx_pending = adapter->tx_ring_count;
}

#ifdef HAVE_ETHTOOL_EXTENDED_RINGPARAMS
/**
 * ixgbevf_set_ringparam - Set ring parameters for a network device
 * @netdev: Pointer to the network device structure
 * @ring: Pointer to the ethtool ring parameters structure containing new settings
 * @ker: Unused pointer to the kernel ethtool ring parameters structure
 * @extack: Unused pointer to netlink extended ACK structure
 *
 * This function configures the ring parameters for the specified network device
 * based on the settings provided in the ethtool ring parameters structure. It
 * adjusts the number of descriptors for TX and RX rings, ensuring they are within
 * acceptable limits and aligned to required multiples. If the new settings differ
 * from the current configuration, the function reallocates resources and updates
 * the ring counts. The network interface is brought down and back up to apply
 * the changes.
 *
 * The function supports conditional compilation to accommodate different kernel
 * versions, which may require additional parameters for extended acknowledgment.
 *
 * Return: 0 on success, -EINVAL if the settings are invalid, or -ENOMEM if memory
 * allocation fails.
 */
static int
ixgbevf_set_ringparam(struct net_device *netdev,
		      struct ethtool_ringparam *ring,
		      struct kernel_ethtool_ringparam __always_unused *ker,
		      struct netlink_ext_ack __always_unused *extack)
#else
static int ixgbevf_set_ringparam(struct net_device *netdev,
				 struct ethtool_ringparam *ring)
#endif
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
	struct ixgbevf_ring *tx_ring = NULL, *rx_ring = NULL;
	u32 new_rx_count, new_tx_count;
	int i, j, err = 0;

	if ((ring->rx_mini_pending) || (ring->rx_jumbo_pending))
		return -EINVAL;

	if (ring->tx_pending > IXGBEVF_MAX_NUM_DESCRIPTORS ||
	    ring->tx_pending < IXGBEVF_MIN_NUM_DESCRIPTORS ||
	    ring->rx_pending > IXGBEVF_MAX_NUM_DESCRIPTORS ||
	    ring->rx_pending < IXGBEVF_MIN_NUM_DESCRIPTORS) {
		netdev_info(netdev,
			    "Descriptors requested (Tx: %d / Rx: %d) out of range [%d-%d]\n",
			    ring->tx_pending, ring->rx_pending,
			    IXGBEVF_MIN_NUM_DESCRIPTORS,
			    IXGBEVF_MAX_NUM_DESCRIPTORS);
		return -EINVAL;
	}

	new_tx_count = ALIGN(ring->tx_pending,
			     IXGBE_REQ_TX_DESCRIPTOR_MULTIPLE);
	new_rx_count = ALIGN(ring->rx_pending,
			     IXGBE_REQ_RX_DESCRIPTOR_MULTIPLE);

	/* if nothing to do return success */
	if ((new_tx_count == adapter->tx_ring_count) &&
	    (new_rx_count == adapter->rx_ring_count))
		return 0;

	while (test_and_set_bit(__IXGBEVF_RESETTING, &adapter->state))
		msleep(1);

	if (!netif_running(adapter->netdev)) {
		for (i = 0; i < adapter->num_tx_queues; i++)
			adapter->tx_ring[i]->count = new_tx_count;
		for (i = 0; i < adapter->num_xdp_queues; i++)
			adapter->xdp_ring[i]->count = new_tx_count;
		for (i = 0; i < adapter->num_rx_queues; i++)
			adapter->rx_ring[i]->count = new_rx_count;
		adapter->tx_ring_count = new_tx_count;
		adapter->xdp_ring_count = new_tx_count;
		adapter->rx_ring_count = new_rx_count;
		goto clear_reset;
	}

	if (new_tx_count != adapter->tx_ring_count) {
		tx_ring = vmalloc((adapter->num_tx_queues +
				   adapter->num_xdp_queues) * sizeof(*tx_ring));
		if (!tx_ring) {
			err = -ENOMEM;
			goto clear_reset;
		}

		for (i = 0; i < adapter->num_tx_queues; i++) {
			/* clone ring and setup updated count */
			tx_ring[i] = *adapter->tx_ring[i];
			tx_ring[i].count = new_tx_count;
			err = ixgbevf_setup_tx_resources(&tx_ring[i]);
			if (err) {
				while (i) {
					i--;
					ixgbevf_free_tx_resources(&tx_ring[i]);
				}

				vfree(tx_ring);
				tx_ring = NULL;

				goto clear_reset;
			}
		}

		for (j = 0; j < adapter->num_xdp_queues; i++, j++) {
			/* clone ring and setup updated count */
			tx_ring[i] = *adapter->xdp_ring[j];
			tx_ring[i].count = new_tx_count;
			err = ixgbevf_setup_tx_resources(&tx_ring[i]);
			if (err) {
				while (i) {
					i--;
					ixgbevf_free_tx_resources(&tx_ring[i]);
				}

				vfree(tx_ring);
				tx_ring = NULL;

				goto clear_reset;
			}
		}
	}

	if (new_rx_count != adapter->rx_ring_count) {
		rx_ring = vmalloc(adapter->num_rx_queues * sizeof(*rx_ring));
		if (!rx_ring) {
			err = -ENOMEM;
			goto clear_reset;
		}

		for (i = 0; i < adapter->num_rx_queues; i++) {
			/* clone ring and setup updated count */
			rx_ring[i] = *adapter->rx_ring[i];
#ifdef HAVE_XDP_BUFF_RXQ

			/* Clear copied XDP RX-queue info */
			memset(&rx_ring[i].xdp_rxq, 0,
			       sizeof(rx_ring[i].xdp_rxq));
#endif /* HAVE_XDP_BUFF_RXQ */

			rx_ring[i].count = new_rx_count;
			err = ixgbevf_setup_rx_resources(adapter, &rx_ring[i]);
			if (err) {
				while (i) {
					i--;
					ixgbevf_free_rx_resources(&rx_ring[i]);
				}

				vfree(rx_ring);
				rx_ring = NULL;

				goto clear_reset;
			}
		}
	}

	/* bring interface down to prepare for update */
	ixgbevf_down(adapter);

	/* Tx */
	if (tx_ring) {
		for (i = 0; i < adapter->num_tx_queues; i++) {
			ixgbevf_free_tx_resources(adapter->tx_ring[i]);
			*adapter->tx_ring[i] = tx_ring[i];
		}
		adapter->tx_ring_count = new_tx_count;

		for (j = 0; j < adapter->num_xdp_queues; i++, j++) {
			ixgbevf_free_tx_resources(adapter->xdp_ring[j]);
			*adapter->xdp_ring[j] = tx_ring[i];
		}
		adapter->xdp_ring_count = new_tx_count;

		vfree(tx_ring);
		tx_ring = NULL;
	}

	/* Rx */
	if (rx_ring) {
		for (i = 0; i < adapter->num_rx_queues; i++) {
			ixgbevf_free_rx_resources(adapter->rx_ring[i]);
			*adapter->rx_ring[i] = rx_ring[i];
		}
		adapter->rx_ring_count = new_rx_count;

		vfree(rx_ring);
		rx_ring = NULL;
	}

	/* restore interface using new values */
	ixgbevf_up(adapter);

clear_reset:
	/* free Tx resources if Rx error is encountered */
	if (tx_ring) {
		for (i = 0;
		     i < adapter->num_tx_queues + adapter->num_xdp_queues; i++)
			ixgbevf_free_tx_resources(&tx_ring[i]);
		vfree(tx_ring);
	}

	clear_bit(__IXGBEVF_RESETTING, &adapter->state);
	return err;
}

/**
 * ixgbevf_get_ethtool_stats - Retrieve ethtool statistics for a network device
 * @netdev: Pointer to the network device structure
 * @stats: Unused pointer to the ethtool stats structure
 * @data: Pointer to an array where the statistics data will be stored
 *
 * This function collects and populates the provided data array with statistics
 * related to the network device. The statistics are typically used for
 * monitoring and diagnostic purposes. The @stats parameter is marked as
 * `__always_unused`, indicating that it is not currently utilized in this
 * function.
 */
static void ixgbevf_get_ethtool_stats(struct net_device *netdev,
				      struct ethtool_stats __always_unused *stats,
				      u64 *data)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
#ifdef HAVE_NDO_GET_STATS64
	struct rtnl_link_stats64 temp;
	const struct rtnl_link_stats64 *net_stats;
	unsigned int start;
#else
	struct net_device_stats *net_stats;
#ifdef HAVE_NET_DEVICE_OPS
	const struct net_device_ops *ops = netdev->netdev_ops;
#endif /* HAVE_NET_DEVICE_OPS */
#endif /* HAVE_NDO_GET_STATS64 */
	struct ixgbevf_ring *ring;
	int i, j;
	char *p;

	ixgbevf_update_stats(adapter);
#ifdef HAVE_NDO_GET_STATS64
	net_stats = dev_get_stats(netdev, &temp);
#else
#ifdef HAVE_NET_DEVICE_OPS
	net_stats = ops->ndo_get_stats(netdev);
#else
	net_stats = netdev->get_stats(netdev);
#endif /* HAVE_NET_DEVICE_OPS */
#endif /* HAVE_NDO_GET_STATS64 */
	for (i = 0; i < IXGBEVF_GLOBAL_STATS_LEN; i++) {
		switch (ixgbe_gstrings_stats[i].type) {
		case NETDEV_STATS:
			p = (char *)net_stats +
					ixgbe_gstrings_stats[i].stat_offset;
			break;
		case IXGBEVF_STATS:
			p = (char *)adapter +
					ixgbe_gstrings_stats[i].stat_offset;
			break;
		default:
			data[i] = 0;
			continue;
		}

		data[i] = (ixgbe_gstrings_stats[i].sizeof_stat ==
			   sizeof(u64)) ? *(u64 *)p : *(u32 *)p;
	}

	/* populate Tx queue data */
	for (j = 0; j < adapter->num_tx_queues; j++) {
		ring = adapter->tx_ring[j];
		if (!ring) {
			data[i++] = 0;
			data[i++] = 0;
#ifdef BP_EXTENDED_STATS
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
#endif
			continue;
		}

#ifdef HAVE_NDO_GET_STATS64
		do {
			start = u64_stats_fetch_begin(&ring->syncp);
#endif
			data[i]   = ring->stats.packets;
			data[i+1] = ring->stats.bytes;
#ifdef HAVE_NDO_GET_STATS64
		} while (u64_stats_fetch_retry(&ring->syncp, start));
#endif
		i += 2;
#ifdef BP_EXTENDED_STATS
		data[i] = ring->stats.yields;
		data[i+1] = ring->stats.misses;
		data[i+2] = ring->stats.cleaned;
		i += 3;
#endif
	}

	/* populate XDP queue data */
	for (j = 0; j < adapter->num_xdp_queues; j++) {
		ring = adapter->xdp_ring[j];
		if (!ring) {
			data[i++] = 0;
			data[i++] = 0;
			continue;
		}

#ifdef HAVE_NDO_GET_STATS64
		do {
			start = u64_stats_fetch_begin(&ring->syncp);
#endif
			data[i] = ring->stats.packets;
			data[i + 1] = ring->stats.bytes;
#ifdef HAVE_NDO_GET_STATS64
		} while (u64_stats_fetch_retry(&ring->syncp, start));
#endif
		i += 2;
	}

	/* populate Rx queue data */
	for (j = 0; j < adapter->num_rx_queues; j++) {
		ring = adapter->rx_ring[j];
		if (!ring) {
			data[i++] = 0;
			data[i++] = 0;
#ifdef BP_EXTENDED_STATS
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
#endif
			continue;
		}

#ifdef HAVE_NDO_GET_STATS64
		do {
			start = u64_stats_fetch_begin(&ring->syncp);
#endif
			data[i]   = ring->stats.packets;
			data[i+1] = ring->stats.bytes;
#ifdef HAVE_NDO_GET_STATS64
		} while (u64_stats_fetch_retry(&ring->syncp, start));
#endif
		i += 2;
#ifdef BP_EXTENDED_STATS
		data[i] = ring->stats.yields;
		data[i+1] = ring->stats.misses;
		data[i+2] = ring->stats.cleaned;
		i += 3;
#endif
	}
}

/**
 * ixgbevf_get_strings - Populate string set data for a network device
 * @netdev: Pointer to the network device structure
 * @stringset: Identifier for the string set to populate
 * @data: Pointer to the buffer where the string data will be stored
 *
 * This function populates the provided buffer with strings corresponding to
 * the specified string set for the network device. The string sets are used
 * by ethtool to provide various types of information about the device. The
 * function supports the following string sets:
 *
 * - ETH_SS_TEST: Populates the buffer with test strings (if not removed by
 *   compatibility settings).
 * - ETH_SS_STATS: Populates the buffer with statistics strings, including
 *   global statistics and per-queue statistics for TX, XDP, and RX queues.
 * - ETH_SS_PRIV_FLAGS: Populates the buffer with private flag strings (if
 *   supported by the kernel configuration).
 *
 * The function uses conditional compilation to include or exclude certain
 * string sets based on the build configuration.
 */
static void ixgbevf_get_strings(struct net_device *netdev, u32 stringset,
				u8 *data)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
	char *p = (char *)data;
	int i;

	switch (stringset) {
	case ETH_SS_TEST:
		memcpy(data, *ixgbe_gstrings_test,
		       IXGBEVF_TEST_LEN * ETH_GSTRING_LEN);
		break;
	case ETH_SS_STATS:
		for (i = 0; i < IXGBEVF_GLOBAL_STATS_LEN; i++) {
			memcpy(p, ixgbe_gstrings_stats[i].stat_string,
			       ETH_GSTRING_LEN);
			p += ETH_GSTRING_LEN;
		}

		for (i = 0; i < adapter->num_tx_queues; i++) {
			snprintf(p, ETH_GSTRING_LEN,
				 "tx_queue_%u_packets", i);
			p += ETH_GSTRING_LEN;
			snprintf(p, ETH_GSTRING_LEN,
				 "tx_queue_%u_bytes", i);
			p += ETH_GSTRING_LEN;
#ifdef BP_EXTENDED_STATS
			snprintf(p, ETH_GSTRING_LEN,
				 "tx_queue_%u_bp_napi_yield", i);
			p += ETH_GSTRING_LEN;
			snprintf(p, ETH_GSTRING_LEN,
				 "tx_queue_%u_bp_misses", i);
			p += ETH_GSTRING_LEN;
			snprintf(p, ETH_GSTRING_LEN,
				 "tx_queue_%u_bp_cleaned", i);
			p += ETH_GSTRING_LEN;
#endif /* BP_EXTENDED_STATS */
		}
		for (i = 0; i < adapter->num_xdp_queues; i++) {
			snprintf(p, ETH_GSTRING_LEN,
				 "xdp_queue_%u_packets", i);
			p += ETH_GSTRING_LEN;
			snprintf(p, ETH_GSTRING_LEN,
				 "xdp_queue_%u_bytes", i);
			p += ETH_GSTRING_LEN;
		}
		for (i = 0; i < adapter->num_rx_queues; i++) {
			snprintf(p, ETH_GSTRING_LEN,
				 "rx_queue_%u_packets", i);
			p += ETH_GSTRING_LEN;
			snprintf(p, ETH_GSTRING_LEN,
				 "rx_queue_%u_bytes", i);
			p += ETH_GSTRING_LEN;
#ifdef BP_EXTENDED_STATS
			snprintf(p, ETH_GSTRING_LEN,
				 "rx_queue_%u_bp_poll_yield", i);
			p += ETH_GSTRING_LEN;
			snprintf(p, ETH_GSTRING_LEN,
				 "rx_queue_%u_bp_misses", i);
			p += ETH_GSTRING_LEN;
			snprintf(p, ETH_GSTRING_LEN,
				 "rx_queue_%u_bp_cleaned", i);
			p += ETH_GSTRING_LEN;
#endif /* BP_EXTENDED_STATS */
		}
		break;
#if defined(HAVE_ETHTOOL_GET_SSET_COUNT) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
	case ETH_SS_PRIV_FLAGS:
		memcpy(data, ixgbevf_priv_flags_strings,
		       IXGBEVF_PRIV_FLAGS_STR_LEN * ETH_GSTRING_LEN);
		break;
#endif /* HAVE_ETHTOOL_GET_SSET_COUNT && HAVE_SWIOTLB_SKIP_CPU_SYNC */
	}
}

static int ixgbevf_link_test(struct ixgbevf_adapter *adapter, u64 *data)
{
	struct ixgbe_hw *hw = &adapter->hw;
	bool link_up;
	u32 link_speed = 0;
	*data = 0;

	hw->mac.ops.check_link(hw, &link_speed, &link_up, true);
	if (link_up)
		return *data;
	else
		*data = 1;
	return *data;
}

/* ethtool register test data */
struct ixgbevf_reg_test {
	u16 reg;
	u8  array_len;
	u8  test_type;
	u32 mask;
	u32 write;
};

/* In the hardware, registers are laid out either singly, in arrays
 * spaced 0x40 bytes apart, or in contiguous tables.  We assume
 * most tests take place on arrays or single registers (handled
 * as a single-element array) and special-case the tables.
 * Table tests are always pattern tests.
 *
 * We also make provision for some required setup steps by specifying
 * registers to be written without any read-back testing.
 */

#define PATTERN_TEST	1
#define SET_READ_TEST	2
#define WRITE_NO_TEST	3
#define TABLE32_TEST	4
#define TABLE64_TEST_LO	5
#define TABLE64_TEST_HI	6

/* default VF register test */
static struct ixgbevf_reg_test reg_test_vf[] = {
	{ IXGBE_VFRDBAL(0), 2, PATTERN_TEST, 0xFFFFFF80, 0xFFFFFF80 },
	{ IXGBE_VFRDBAH(0), 2, PATTERN_TEST, 0xFFFFFFFF, 0xFFFFFFFF },
	{ IXGBE_VFRDLEN(0), 2, PATTERN_TEST, 0x000FFF80, 0x000FFFFF },
	{ IXGBE_VFRXDCTL(0), 2, WRITE_NO_TEST, 0, IXGBE_RXDCTL_ENABLE },
	{ IXGBE_VFRDT(0), 2, PATTERN_TEST, 0x0000FFFF, 0x0000FFFF },
	{ IXGBE_VFRXDCTL(0), 2, WRITE_NO_TEST, 0, 0 },
	{ IXGBE_VFTDBAL(0), 2, PATTERN_TEST, 0xFFFFFF80, 0xFFFFFFFF },
	{ IXGBE_VFTDBAH(0), 2, PATTERN_TEST, 0xFFFFFFFF, 0xFFFFFFFF },
	{ IXGBE_VFTDLEN(0), 2, PATTERN_TEST, 0x000FFF80, 0x000FFF80 },
	{ 0, 0, 0, 0, 0 }
};

static int
reg_pattern_test(struct ixgbevf_adapter *adapter, u32 r, u32 m, u32 w,
		 u64 *data)
{
	static const u32 _test[] = {
		0x5A5A5A5A, 0xA5A5A5A5, 0x00000000, 0xFFFFFFFF
	};
	struct ixgbe_hw *hw = &adapter->hw;
	u32 pat, val, before;

	if (IXGBE_REMOVED(hw->hw_addr)) {
		*data = 1;
		return 1;
	}
	for (pat = 0; pat < ARRAY_SIZE(_test); pat++) {
		before = IXGBE_READ_REG(hw, r);
		IXGBE_WRITE_REG(hw, r, _test[pat] & w);
		val = IXGBE_READ_REG(hw, r);
		if (val != (_test[pat] & w & m)) {
			DPRINTK(DRV, ERR,
			      "pattern test reg %04X failed: got 0x%08X expected 0x%08X\n",
			      r, val, _test[pat] & w & m);
			*data = r;
			IXGBE_WRITE_REG(hw, r, before);
			return 1;
		}
		IXGBE_WRITE_REG(hw, r, before);
	}
	return 0;
}

static int
reg_set_and_check(struct ixgbevf_adapter *adapter, u32 r, u32 m, u32 w,
		  u64 *data)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 val, before;

	if (IXGBE_REMOVED(hw->hw_addr)) {
		*data = 1;
		return 1;
	}
	before = IXGBE_READ_REG(hw, r);
	IXGBE_WRITE_REG(hw, r, w & m);
	val = IXGBE_READ_REG(hw, r);
	if ((w & m) != (val & m)) {
		DPRINTK(DRV, ERR,
		      "set/check reg %04X test failed: got 0x%08X expected 0x%08X\n",
		      r, (val & m), (w & m));
		*data = r;
		IXGBE_WRITE_REG(hw, r, before);
		return 1;
	}
	IXGBE_WRITE_REG(hw, r, before);
	return 0;
}

static int ixgbevf_reg_test(struct ixgbevf_adapter *adapter, u64 *data)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct ixgbevf_reg_test *test;
	int rc;
	u32 i;

	if (IXGBE_REMOVED(hw->hw_addr)) {
		DPRINTK(DRV, ERR, "Adapter removed - register test blocked\n");
		*data = 1;
		return 1;
	}
	test = reg_test_vf;

	/*
	 * Perform the register test, looping through the test table
	 * until we either fail or reach the null entry.
	 */
	while (test->reg) {
		for (i = 0; i < test->array_len; i++) {
			rc = 0;
			switch (test->test_type) {
			case PATTERN_TEST:
				rc = reg_pattern_test(adapter,
						      test->reg + (i * 0x40),
						      test->mask,
						      test->write,
						      data);
				break;
			case SET_READ_TEST:
				rc = reg_set_and_check(adapter,
						       test->reg + (i * 0x40),
						       test->mask,
						       test->write,
						       data);
				break;
			case WRITE_NO_TEST:
				IXGBE_WRITE_REG(hw, test->reg + (i * 0x40),
						test->write);
				break;
			case TABLE32_TEST:
				rc = reg_pattern_test(adapter,
						      test->reg + (i * 4),
						      test->mask,
						      test->write,
						      data);
				break;
			case TABLE64_TEST_LO:
				rc = reg_pattern_test(adapter,
						      test->reg + (i * 8),
						      test->mask,
						      test->write,
						      data);
				break;
			case TABLE64_TEST_HI:
				rc = reg_pattern_test(adapter,
						      test->reg + 4 + (i * 8),
						      test->mask,
						      test->write,
						      data);
				break;
			}
			if (rc)
				return rc;
		}
		test++;
	}

	*data = 0;
	return *data;
}

#ifdef HAVE_ETHTOOL_GET_SSET_COUNT
/**
 * ixgbevf_get_sset_count - Get the count of strings in a specified string set
 * @netdev: Pointer to the network device structure
 * @stringset: Identifier for the string set to query
 *
 * This function returns the number of strings in a specified string set for
 * the network device. The string sets are used by ethtool to provide various
 * types of information about the device. The function supports the following
 * string sets:
 *
 * - ETH_SS_TEST: Returns the number of test strings.
 * - ETH_SS_STATS: Returns the number of statistics strings.
 * - ETH_SS_PRIV_FLAGS: Returns the number of private flag strings (if supported).
 *
 * If the specified string set is not recognized, the function returns -EINVAL.
 *
 * Return: The number of strings in the specified string set, or -EINVAL if the
 * string set is not supported.
 */
static int ixgbevf_get_sset_count(struct net_device *netdev, int stringset)
{
	switch(stringset) {
	case ETH_SS_TEST:
		return IXGBEVF_TEST_LEN;
	case ETH_SS_STATS:
		return IXGBEVF_STATS_LEN;
#ifdef HAVE_SWIOTLB_SKIP_CPU_SYNC
	case ETH_SS_PRIV_FLAGS:
		return IXGBEVF_PRIV_FLAGS_STR_LEN;
#endif /* HAVE_SWIOTLB_SKIP_CPU_SYNC */
	default:
		return -EINVAL;
	}
}
#else
/**
 * ixgbevf_diag_test_count - Get the number of diagnostic tests available
 * @netdev: Unused pointer to the network device structure
 *
 * This function returns the number of diagnostic tests available for the
 * ixgbevf driver. The function does not use the @netdev parameter, as it
 * is marked with the __always_unused attribute, indicating that it is
 * intentionally unused.
 *
 * Return: The number of diagnostic tests available.
 */
static int ixgbevf_diag_test_count(struct net_device __always_unused *netdev)
{
	return IXGBEVF_TEST_LEN;
}

/**
 * ixgbevf_get_stats_count - Get the count of statistics strings
 * @netdev: Pointer to the network device structure
 *
 * This function returns the number of statistics strings available for the
 * specified network device. These statistics strings are used by ethtool to
 * provide detailed information about the device's performance and status.
 *
 * Return: The number of statistics strings as defined by IXGBEVF_STATS_LEN.
 */
static int ixgbevf_get_stats_count(struct net_device *netdev)
{
	return IXGBEVF_STATS_LEN;
}
#endif

/**
 * ixgbevf_diag_test - Perform diagnostic tests on the network device
 * @netdev: Pointer to the network device structure
 * @eth_test: Pointer to the ethtool test structure containing test settings
 * @data: Pointer to an array where test results will be stored
 *
 * This function performs a series of diagnostic tests on the specified network
 * device. The results of these tests are stored in the array pointed to by
 * @data. The tests are configured based on the settings provided in the
 * @eth_test structure. This function is typically used to assess the health
 * and performance of the network device.
 */
static void ixgbevf_diag_test(struct net_device *netdev,
			      struct ethtool_test *eth_test, u64 *data)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
	bool if_running = netif_running(netdev);

	if (IXGBE_REMOVED(adapter->hw.hw_addr)) {
		DPRINTK(DRV, ERR, "Adapter removed - test blocked\n");
		eth_test->flags |= ETH_TEST_FL_FAILED;
		data[0] = 1;
		data[1] = 1;
		return;
	}
	set_bit(__IXGBEVF_TESTING, &adapter->state);
	if (eth_test->flags == ETH_TEST_FL_OFFLINE) {
		/* Offline tests */

		DPRINTK(HW, INFO, "offline testing starting\n");

		/* Link test performed before hardware reset so autoneg doesn't
		 * interfere with test result */
		if (ixgbevf_link_test(adapter, &data[1]))
			eth_test->flags |= ETH_TEST_FL_FAILED;

		if (if_running)
			/* indicate we're in test mode */
			ixgbevf_close(netdev);
		else
			ixgbevf_reset(adapter);

		DPRINTK(HW, INFO, "register testing starting\n");
		if (ixgbevf_reg_test(adapter, &data[0]))
			eth_test->flags |= ETH_TEST_FL_FAILED;

		ixgbevf_reset(adapter);

		clear_bit(__IXGBEVF_TESTING, &adapter->state);
		if (if_running)
			ixgbevf_open(netdev);
	} else {
		DPRINTK(HW, INFO, "online testing starting\n");
		/* Online tests */
		if (ixgbevf_link_test(adapter, &data[1]))
			eth_test->flags |= ETH_TEST_FL_FAILED;

		/* Online tests aren't run; pass by default */
		data[0] = 0;

		clear_bit(__IXGBEVF_TESTING, &adapter->state);
	}
	msleep_interruptible(4 * 1000);
}

/**
 * ixgbevf_nway_reset - Perform a NWay reset on the network device
 * @netdev: Pointer to the network device structure
 *
 * This function performs a NWay reset on the specified network device. A NWay
 * reset is typically used to restart the autonegotiation process for the
 * network link. If the network device is currently running, the function
 * reinitializes the adapter by calling `ixgbevf_reinit_locked`.
 *
 * Return: 0 on success.
 */
static int ixgbevf_nway_reset(struct net_device *netdev)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);

	if (netif_running(netdev))
		ixgbevf_reinit_locked(adapter);

	return 0;
}

/**
 * ixgbevf_get_coalesce - Retrieve interrupt coalescing settings
 * @netdev: Pointer to the network device structure
 * @ec: Pointer to the ethtool coalesce structure to be filled with settings
 * @kernel_coal: Pointer to the kernel ethtool coalesce structure (optional)
 * @extack: Pointer to netlink extended ACK structure for error reporting (optional)
 *
 * This function retrieves the current interrupt coalescing settings for the
 * specified network device and fills the provided ethtool coalesce structure
 * with these settings. The function supports conditional compilation to
 * accommodate different kernel versions. If the kernel supports extended
 * acknowledgment, additional parameters for kernel coalesce settings and
 * netlink extended ACK are included.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int ixgbevf_get_coalesce(struct net_device *netdev,
#ifdef HAVE_ETHTOOL_COALESCE_EXTACK
				struct ethtool_coalesce *ec,
				struct kernel_ethtool_coalesce *kernel_coal,
				struct netlink_ext_ack *extack)
#else
				struct ethtool_coalesce *ec)
#endif
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);

	/* only valid if in constant ITR mode */
	if (adapter->rx_itr_setting <= 1)
		ec->rx_coalesce_usecs = adapter->rx_itr_setting;
	else
		ec->rx_coalesce_usecs = adapter->rx_itr_setting >> 2;

	/* if in mixed tx/rx queues per vector mode, report only rx settings */
	if (adapter->q_vector[0]->tx.count && adapter->q_vector[0]->rx.count)
		return 0;

	/* only valid if in constant ITR mode */
	if (adapter->tx_itr_setting <= 1)
		ec->tx_coalesce_usecs = adapter->tx_itr_setting;
	else
		ec->tx_coalesce_usecs = adapter->tx_itr_setting >> 2;

	return 0;
}

/**
 * ixgbevf_set_coalesce - Set interrupt coalescing parameters for a network device
 * @netdev: Pointer to the network device structure
 * @ec: Pointer to the ethtool coalesce structure containing new settings
 * @kernel_coal: Pointer to the kernel ethtool coalesce structure (optional)
 * @extack: Pointer to netlink extended ACK structure for error reporting (optional)
 *
 * This function configures the interrupt coalescing parameters for the specified
 * network device based on the settings provided in the ethtool coalesce structure.
 * It adjusts the RX and TX interrupt throttling rates (ITR) for each queue vector
 * in the adapter. The function ensures that the coalescing settings are within
 * acceptable limits and applies the changes to the hardware.
 *
 * The function supports conditional compilation to accommodate different kernel
 * versions, which may require additional parameters for extended acknowledgment.
 *
 * Return: 0 on success, -EINVAL if the settings are invalid.
 */
static int ixgbevf_set_coalesce(struct net_device *netdev,
#ifdef HAVE_ETHTOOL_COALESCE_EXTACK
				struct ethtool_coalesce *ec,
				struct kernel_ethtool_coalesce *kernel_coal,
				struct netlink_ext_ack *extack)
#else
				struct ethtool_coalesce *ec)
#endif
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
	struct ixgbevf_q_vector *q_vector;
	int i;
	u16 tx_itr_param, rx_itr_param;

	/* don't accept tx specific changes if we've got mixed RxTx vectors */
	if (adapter->q_vector[0]->tx.count && adapter->q_vector[0]->rx.count
	    && ec->tx_coalesce_usecs)
		return -EINVAL;


	if ((ec->rx_coalesce_usecs > (IXGBE_MAX_EITR >> 2)) ||
	    (ec->tx_coalesce_usecs > (IXGBE_MAX_EITR >> 2)))
		return -EINVAL;

	if (ec->rx_coalesce_usecs > 1)
		adapter->rx_itr_setting = ec->rx_coalesce_usecs << 2;
	else
		adapter->rx_itr_setting = ec->rx_coalesce_usecs;

	if (adapter->rx_itr_setting == 1)
		rx_itr_param = IXGBE_20K_ITR;
	else
		rx_itr_param = adapter->rx_itr_setting;


	if (ec->tx_coalesce_usecs > 1)
		adapter->tx_itr_setting = ec->tx_coalesce_usecs << 2;
	else
		adapter->tx_itr_setting = ec->tx_coalesce_usecs;

	if (adapter->tx_itr_setting == 1)
		tx_itr_param = IXGBE_12K_ITR;
	else
		tx_itr_param = adapter->tx_itr_setting;

	for (i = 0; i < adapter->num_q_vectors; i++) {
		q_vector = adapter->q_vector[i];
		if (q_vector->tx.count && !q_vector->rx.count)
			/* tx only */
			q_vector->itr = tx_itr_param;
		else
			/* rx only or mixed */
			q_vector->itr = rx_itr_param;
		ixgbevf_write_eitr(q_vector);
	}

	return 0;
}

#ifdef ETHTOOL_GRXRINGS
#define UDP_RSS_FLAGS (IXGBEVF_FLAG_RSS_FIELD_IPV4_UDP | \
		       IXGBEVF_FLAG_RSS_FIELD_IPV6_UDP)
static int ixgbevf_set_rss_hash_opt(struct ixgbevf_adapter *adapter,
				    struct ethtool_rxnfc *nfc)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 flags = adapter->flags;


	if (hw->mac.type < ixgbe_mac_X550)
		return -EOPNOTSUPP;

	/* RSS does not support anything other than hashing
	 * to queues on src and dst IPs and ports
	 */
	if (nfc->data & ~(RXH_IP_SRC | RXH_IP_DST |
			  RXH_L4_B_0_1 | RXH_L4_B_2_3))
		return -EINVAL;

	switch (nfc->flow_type) {
	case TCP_V4_FLOW:
	case TCP_V6_FLOW:
		if (!(nfc->data & RXH_IP_SRC) ||
		    !(nfc->data & RXH_IP_DST) ||
		    !(nfc->data & RXH_L4_B_0_1) ||
		    !(nfc->data & RXH_L4_B_2_3))
			return -EINVAL;
		break;
	case UDP_V4_FLOW:
		if (!(nfc->data & RXH_IP_SRC) ||
		    !(nfc->data & RXH_IP_DST))
			return -EINVAL;
		switch (nfc->data & (RXH_L4_B_0_1 | RXH_L4_B_2_3)) {
		case 0:
			flags &= ~IXGBEVF_FLAG_RSS_FIELD_IPV4_UDP;
			break;
		case (RXH_L4_B_0_1 | RXH_L4_B_2_3):
			flags |= IXGBEVF_FLAG_RSS_FIELD_IPV4_UDP;
			break;
		default:
			return -EINVAL;
		}
		break;
	case UDP_V6_FLOW:
		if (!(nfc->data & RXH_IP_SRC) ||
		    !(nfc->data & RXH_IP_DST))
			return -EINVAL;
		switch (nfc->data & (RXH_L4_B_0_1 | RXH_L4_B_2_3)) {
		case 0:
			flags &= ~IXGBEVF_FLAG_RSS_FIELD_IPV6_UDP;
			break;
		case (RXH_L4_B_0_1 | RXH_L4_B_2_3):
			flags |= IXGBEVF_FLAG_RSS_FIELD_IPV6_UDP;
			break;
		default:
			return -EINVAL;
		}
		break;
	case AH_ESP_V4_FLOW:
	case AH_V4_FLOW:
	case ESP_V4_FLOW:
	case SCTP_V4_FLOW:
	case AH_ESP_V6_FLOW:
	case AH_V6_FLOW:
	case ESP_V6_FLOW:
	case SCTP_V6_FLOW:
		if (!(nfc->data & RXH_IP_SRC) ||
		    !(nfc->data & RXH_IP_DST) ||
		    (nfc->data & RXH_L4_B_0_1) ||
		    (nfc->data & RXH_L4_B_2_3))
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	/* if we changed something we need to update flags */
	if (flags != adapter->flags) {
		u32 vfmrqc;

		vfmrqc = IXGBE_READ_REG(hw, IXGBE_VFMRQC);

		if ((flags & UDP_RSS_FLAGS) &&
		    !(adapter->flags & UDP_RSS_FLAGS))
			DPRINTK(DRV, WARNING, "enabling UDP RSS: fragmented packets may arrive out of order to the stack above\n");

		adapter->flags = flags;

		/* Perform hash on these packet types */
		vfmrqc |= IXGBE_MRQC_RSS_FIELD_IPV4
		      | IXGBE_MRQC_RSS_FIELD_IPV4_TCP
		      | IXGBE_MRQC_RSS_FIELD_IPV6
		      | IXGBE_MRQC_RSS_FIELD_IPV6_TCP;

		vfmrqc &= ~(IXGBE_MRQC_RSS_FIELD_IPV4_UDP |
			  IXGBE_MRQC_RSS_FIELD_IPV6_UDP);

		if (flags & IXGBEVF_FLAG_RSS_FIELD_IPV4_UDP)
			vfmrqc |= IXGBE_MRQC_RSS_FIELD_IPV4_UDP;

		if (flags & IXGBEVF_FLAG_RSS_FIELD_IPV6_UDP)
			vfmrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_UDP;

		IXGBE_WRITE_REG(hw, IXGBE_VFMRQC, vfmrqc);
	}

	return 0;
}

static int ixgbevf_get_rss_hash_opts(struct ixgbevf_adapter *adapter,
				   struct ethtool_rxnfc *cmd)
{
	cmd->data = 0;

	/* Report default options for RSS on ixgbevf */
	switch (cmd->flow_type) {
	case TCP_V4_FLOW:
		cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
		fallthrough;
	case UDP_V4_FLOW:
		if (adapter->flags & IXGBEVF_FLAG_RSS_FIELD_IPV4_UDP)
			cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
		fallthrough;
	case SCTP_V4_FLOW:
	case AH_ESP_V4_FLOW:
	case AH_V4_FLOW:
	case ESP_V4_FLOW:
	case IPV4_FLOW:
		cmd->data |= RXH_IP_SRC | RXH_IP_DST;
		break;
	case TCP_V6_FLOW:
		cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
		fallthrough;
	case UDP_V6_FLOW:
		if (adapter->flags & IXGBEVF_FLAG_RSS_FIELD_IPV6_UDP)
			cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
		fallthrough;
	case SCTP_V6_FLOW:
	case AH_ESP_V6_FLOW:
	case AH_V6_FLOW:
	case ESP_V6_FLOW:
	case IPV6_FLOW:
		cmd->data |= RXH_IP_SRC | RXH_IP_DST;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * ixgbevf_set_rxnfc - Set RX network flow classification options
 * @dev: Pointer to the network device structure
 * @cmd: Pointer to the ethtool RX network flow classification command structure
 *
 * This function sets the receive (RX) network flow classification options for
 * the specified network device based on the provided ethtool command. It
 * currently supports setting the RSS (Receive Side Scaling) hash options when
 * the command is ETHTOOL_SRXFH. If the command is not supported, the function
 * returns -EOPNOTSUPP.
 *
 * Return: 0 on success, -EOPNOTSUPP if the command is not supported.
 */
static int ixgbevf_set_rxnfc(struct net_device *dev, struct ethtool_rxnfc *cmd)
{
	struct ixgbevf_adapter *adapter = netdev_priv(dev);
	int ret = -EOPNOTSUPP;

	if (cmd->cmd == ETHTOOL_SRXFH)
		ret = ixgbevf_set_rss_hash_opt(adapter, cmd);

	return ret;
}

/**
 * ixgbevf_get_rxnfc - Retrieve RX network flow classification settings
 * @dev: Pointer to the network device structure
 * @info: Pointer to the ethtool RX network flow classification structure
 * @rule_locs: Unused pointer to rule locations (type depends on kernel version)
 *
 * This function retrieves various RX network flow classification settings for
 * the specified network device. It supports different commands to provide
 * specific information:
 *
 * - ETHTOOL_GRXRINGS: Fills the @info structure with the number of RX rings
 *   (queues) available on the device.
 * - ETHTOOL_GRXFH: Retrieves the RSS (Receive Side Scaling) hash options and
 *   fills the @info structure with these settings.
 *
 * The function supports conditional compilation to accommodate different kernel
 * versions, which may require different types for the rule locations parameter.
 *
 * Return: 0 on success, -EOPNOTSUPP if the command is not supported, or a
 * negative error code on failure.
 */
static int ixgbevf_get_rxnfc(struct net_device *dev, struct ethtool_rxnfc *info,
#ifdef HAVE_ETHTOOL_GET_RXNFC_VOID_RULE_LOCS
			     __always_unused void *rule_locs)
#else
			     __always_unused u32 *rule_locs)
#endif
{
	struct ixgbevf_adapter *adapter = netdev_priv(dev);
	int ret;

	switch (info->cmd) {
	case ETHTOOL_GRXRINGS:
		info->data = adapter->num_rx_queues;
		break;
	case ETHTOOL_GRXFH:
		ret = ixgbevf_get_rss_hash_opts(adapter, info);
		if (ret)
			return ret;
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}
#endif /* ETHTOOL_GRXRINGS */

#if defined(ETHTOOL_GRSSH) && defined(ETHTOOL_SRSSH)
/**
 * ixgbevf_get_reta_locked - get the RSS redirection table (RETA) contents.
 * @hw: pointer to the HW structure
 * @reta: buffer to fill with RETA contents.
 * @num_rx_queues: Number of Rx queues configured for this port
 *
 * The "reta" buffer should be big enough to contain 32 registers.
 *
 * Returns: 0 on success.
 *          if API doesn't support this operation - (-EOPNOTSUPP).
 */
static int ixgbevf_get_reta_locked(struct ixgbe_hw *hw, u32 *reta,
				   int num_rx_queues)
{
	int err, i, j;
	u32 msgbuf[IXGBE_VFMAILBOX_SIZE];
	u32 *hw_reta = &msgbuf[1];
	u32 mask = 0;

	/* We have to use a mailbox for 82599 and x540 devices only.
	 * For these devices RETA has 128 entries.
	 * Also these VFs support up to 4 RSS queues. Therefore PF will compress
	 * 16 RETA entries in each DWORD giving 2 bits to each entry.
	 */
	int dwords = IXGBEVF_82599_RETA_SIZE / 16;

	/* We support the RSS querying for 82599 and x540 devices only.
	 * Thus return an error if API doesn't support RETA querying or querying
	 * is not supported for this device type.
	 */
	switch (hw->api_version) {
	case ixgbe_mbox_api_17:
	case ixgbe_mbox_api_16:
	case ixgbe_mbox_api_15:
	case ixgbe_mbox_api_13:
	case ixgbe_mbox_api_12:
		if (hw->mac.type < ixgbe_mac_X550_vf)
			break;
		fallthrough;
	default:
		return -EOPNOTSUPP;
	}

	msgbuf[0] = IXGBE_VF_GET_RETA;

	err = ixgbe_write_mbx(hw, msgbuf, 1, 0);

	if (err)
		return err;

	err = ixgbe_poll_mbx(hw, msgbuf, dwords + 1, 0);

	if (err)
		return err;

	msgbuf[0] &= ~IXGBE_VT_MSGTYPE_CTS;

	/* If the operation has been refused by a PF return -EPERM */
	if (msgbuf[0] == (IXGBE_VF_GET_RETA | IXGBE_VT_MSGTYPE_FAILURE))
		return -EPERM;

	/* If we didn't get an ACK there must have been
	 * some sort of mailbox error so we should treat it
	 * as such.
	 */
	if (msgbuf[0] != (IXGBE_VF_GET_RETA | IXGBE_VT_MSGTYPE_SUCCESS))
		return IXGBE_ERR_MBX;

	/* ixgbevf doesn't support more than 2 queues at the moment */
	if (num_rx_queues > 1)
		mask = 0x1;

	for (i = 0; i < dwords; i++)
		for (j = 0; j < 16; j++)
			reta[i * 16 + j] = (hw_reta[i] >> (2 * j)) & mask;

	return 0;
}

/**
 * ixgbevf_get_rss_key_locked - get the RSS Random Key
 * @hw: pointer to the HW structure
 * @rss_key: buffer to fill with RSS Hash Key contents.
 *
 * The "rss_key" buffer should be big enough to contain 10 registers.
 *
 * Returns: 0 on success.
 *          if API doesn't support this operation - (-EOPNOTSUPP).
 */
static int ixgbevf_get_rss_key_locked(struct ixgbe_hw *hw, u8 *rss_key)
{
	int err;
	u32 msgbuf[IXGBE_VFMAILBOX_SIZE];

	/* We currently support the RSS Random Key retrieval for 82599 and x540
	 * devices only.
	 *
	 * Thus return an error if API doesn't support RSS Random Key retrieval
	 * or if the operation is not supported for this device type.
	 */
	switch (hw->api_version) {
	case ixgbe_mbox_api_17:
	case ixgbe_mbox_api_16:
	case ixgbe_mbox_api_15:
	case ixgbe_mbox_api_13:
	case ixgbe_mbox_api_12:
		if (hw->mac.type < ixgbe_mac_X550_vf)
			break;
		fallthrough;
	default:
		return -EOPNOTSUPP;
	}

	msgbuf[0] = IXGBE_VF_GET_RSS_KEY;
	err = ixgbe_write_mbx(hw, msgbuf, 1, 0);

	if (err)
		return err;

	err = ixgbe_poll_mbx(hw, msgbuf, 11, 0);

	if (err)
		return err;

	msgbuf[0] &= ~IXGBE_VT_MSGTYPE_CTS;

	/* If the operation has been refused by a PF return -EPERM */
	if (msgbuf[0] == (IXGBE_VF_GET_RSS_KEY | IXGBE_VT_MSGTYPE_FAILURE))
		return -EPERM;

	/* If we didn't get an ACK there must have been
	 * some sort of mailbox error so we should treat it
	 * as such.
	 */
	if (msgbuf[0] != (IXGBE_VF_GET_RSS_KEY | IXGBE_VT_MSGTYPE_SUCCESS))
		return IXGBE_ERR_MBX;

	memcpy(rss_key, msgbuf + 1, IXGBEVF_RSS_HASH_KEY_SIZE);

	return 0;
}

/**
 * ixgbevf_get_rxfh_indir_size - Get the size of the RX flow hash indirection table
 * @netdev: Pointer to the network device structure
 *
 * This function returns the size of the receive (RX) flow hash indirection
 * table for the specified network device. The indirection table is used in
 * the process of distributing incoming packets across multiple receive queues
 * based on their hash values. Knowing the size of this table is important for
 * configuring and managing the distribution of network traffic.
 *
 * Return: The size of the RX flow hash indirection table as a 32-bit unsigned integer.
 */
static u32 ixgbevf_get_rxfh_indir_size(struct net_device *netdev)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);

	if (adapter->hw.mac.type >= ixgbe_mac_X550_vf)
		return IXGBEVF_X550_VFRETA_SIZE;

	return IXGBEVF_82599_RETA_SIZE;
}

/**
 * ixgbevf_get_rxfh_key_size - Get the size of the RX flow hash key
 * @netdev: Pointer to the network device structure
 *
 * This function returns the size of the receive (RX) flow hash key for the
 * specified network device. The hash key is used in conjunction with the
 * indirection table to determine how incoming packets are distributed across
 * multiple receive queues. Knowing the size of the hash key is important for
 * configuring and managing the distribution of network traffic.
 *
 * Return: The size of the RX flow hash key as a 32-bit unsigned integer.
 */
static u32 ixgbevf_get_rxfh_key_size(struct net_device *netdev)
{
	return IXGBEVF_RSS_HASH_KEY_SIZE;
}

/**
 * ixgbevf_get_rxfh - Retrieve RX flow hash configuration
 * @netdev: Pointer to the network device structure
 * @rxfh: Pointer to the ethtool RX flow hash parameter structure (optional)
 * @indir: Pointer to the indirection table (optional)
 * @key: Pointer to the hash key (optional)
 * @hfunc: Pointer to the hash function (optional)
 *
 * This function retrieves the current RX flow hash configuration for the
 * specified network device. The configuration includes the indirection table,
 * hash key, and hash function used for distributing incoming packets across
 * multiple receive queues. The function supports conditional compilation to
 * accommodate different kernel versions, which may require different parameters.
 *
 * Return: 0 on success, negative error code on failure.
 */
#ifdef HAVE_RXFH_HASHFUNC
#ifdef HAVE_ETHTOOL_RXFH_PARAM
static int ixgbevf_get_rxfh(struct net_device *netdev,
			    struct ethtool_rxfh_param *rxfh)
#else
static int ixgbevf_get_rxfh(struct net_device *netdev, u32 *indir, u8 *key,
			    u8 *hfunc)
#endif /* HAVE_ETHTOOL_RXFH_PARAM */
#else
static int ixgbevf_get_rxfh(struct net_device *netdev, u32 *indir, u8 *key)
#endif
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
#ifdef HAVE_ETHTOOL_RXFH_PARAM
	u32 *indir = rxfh->indir;
	u8 *key = rxfh->key;
#endif /* HAVE_ETHTOOL_RXFH_PARAM */
	int err = 0;

#ifdef HAVE_RXFH_HASHFUNC
#ifdef HAVE_ETHTOOL_RXFH_PARAM
	rxfh->hfunc = ETH_RSS_HASH_TOP;
#else
	if (hfunc)
		*hfunc = ETH_RSS_HASH_TOP;
#endif /* HAVE_ETHTOOL_RXFH_PARAM */
#endif

	if (adapter->hw.mac.type >= ixgbe_mac_X550_vf) {
		if (key)
			memcpy(key, adapter->rss_key,
			       ixgbevf_get_rxfh_key_size(netdev));

		if (indir) {
			int i;

			for (i = 0; i < IXGBEVF_X550_VFRETA_SIZE; i++)
				indir[i] = adapter->rss_indir_tbl[i];
		}
	} else {
		/* If neither indirection table nor hash key was requested
		 *  - just return a success avoiding taking any locks.
		 */
		if (!indir && !key)
			return 0;

		spin_lock_bh(&adapter->mbx_lock);
		if (indir)
			err = ixgbevf_get_reta_locked(&adapter->hw, indir,
						      adapter->num_rx_queues);

		if (!err && key)
			err = ixgbevf_get_rss_key_locked(&adapter->hw, key);

		spin_unlock_bh(&adapter->mbx_lock);
	}

	return err;
}

#endif /* ETHTOOL_GRSSH && ETHTOOL_SRSSH */
#if defined(HAVE_ETHTOOL_GET_SSET_COUNT) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
/**
 * ixgbevf_get_priv_flags - Retrieve private flags for a network device
 * @netdev: Pointer to the network device structure
 *
 * This function returns the current private flags for the specified network
 * device. Private flags are driver-specific settings that can be used to
 * control various aspects of the device's behavior. These flags are typically
 * used for advanced configuration and debugging purposes.
 *
 * Return: The current private flags as a 32-bit unsigned integer.
 */
static u32 ixgbevf_get_priv_flags(struct net_device *netdev)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
	u32 priv_flags = 0;

	if (adapter->flags & IXGBEVF_FLAGS_LEGACY_RX)
		priv_flags |= IXGBEVF_PRIV_FLAGS_LEGACY_RX;

	return priv_flags;
}

/**
 * ixgbevf_set_priv_flags - Set private flags for a network device
 * @netdev: Pointer to the network device structure
 * @priv_flags: The new private flags to be set
 *
 * This function sets the private flags for the specified network device. Private
 * flags are driver-specific settings that can be used to control various aspects
 * of the device's behavior. The function updates the adapter's flags based on
 * the provided @priv_flags, specifically handling the IXGBEVF_FLAGS_LEGACY_RX
 * flag. If the flags are changed, the network interface is reset to repopulate
 * the queues, provided the interface is currently running.
 *
 * Return: 0 on success.
 */
static int ixgbevf_set_priv_flags(struct net_device *netdev, u32 priv_flags)
{
	struct ixgbevf_adapter *adapter = netdev_priv(netdev);
	unsigned int flags = adapter->flags;

	flags &= ~IXGBEVF_FLAGS_LEGACY_RX;
	if (priv_flags & IXGBEVF_PRIV_FLAGS_LEGACY_RX)
		flags |= IXGBEVF_FLAGS_LEGACY_RX;

	if (flags != adapter->flags) {
		adapter->flags = flags;

		/* reset interface to repopulate queues */
		if (netif_running(netdev))
			ixgbevf_reinit_locked(adapter);
	}

	return 0;
}

#endif /* HAVE_ETHTOOL_GET_SSET_COUNT && HAVE_SWIOTLB_SKIP_CPU_SYNC */
static struct ethtool_ops ixgbevf_ethtool_ops = {
#ifdef HAVE_ETHTOOL_CONVERT_U32_AND_LINK_MODE
	.get_link_ksettings	= ixgbevf_get_link_ksettings,
	.set_link_ksettings	= ixgbevf_set_link_ksettings,
#else
	.get_settings           = ixgbevf_get_settings,
	.set_settings           = ixgbevf_set_settings,
#endif
	.get_drvinfo            = ixgbevf_get_drvinfo,
	.get_regs_len           = ixgbevf_get_regs_len,
	.get_regs               = ixgbevf_get_regs,
	.nway_reset             = ixgbevf_nway_reset,
	.get_link               = ethtool_op_get_link,
	.get_eeprom             = ixgbevf_get_eeprom,
	.set_eeprom             = ixgbevf_set_eeprom,
#ifndef HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
#ifdef HAVE_ETHTOOL_GET_TS_INFO
	.get_ts_info		= ethtool_op_get_ts_info,
#endif /* HAVE_ETHTOOL_GET_TS_INFO */
#if defined(ETHTOOL_GRSSH) && defined(ETHTOOL_SRSSH)
	.get_rxfh_indir_size	= ixgbevf_get_rxfh_indir_size,
	.get_rxfh_key_size	= ixgbevf_get_rxfh_key_size,
	.get_rxfh		= ixgbevf_get_rxfh,
#endif /* ETHTOOL_GRSSH && ETHTOOL_SRSSH */
#endif /* HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT */
	.get_ringparam          = ixgbevf_get_ringparam,
	.set_ringparam          = ixgbevf_set_ringparam,
	.get_msglevel           = ixgbevf_get_msglevel,
	.set_msglevel           = ixgbevf_set_msglevel,
#ifndef HAVE_NDO_SET_FEATURES
	.get_rx_csum            = ixgbevf_get_rx_csum,
	.set_rx_csum            = ixgbevf_set_rx_csum,
	.get_tx_csum            = ixgbevf_get_tx_csum,
	.set_tx_csum            = ixgbevf_set_tx_csum,
	.get_sg                 = ethtool_op_get_sg,
	.set_sg                 = ethtool_op_set_sg,
#ifdef NETIF_F_TSO
	.get_tso                = ethtool_op_get_tso,
	.set_tso                = ixgbevf_set_tso,
#endif
#endif
	.self_test              = ixgbevf_diag_test,
#ifdef HAVE_ETHTOOL_GET_SSET_COUNT
	.get_sset_count         = ixgbevf_get_sset_count,
#else
	.self_test_count        = ixgbevf_diag_test_count,
	.get_stats_count        = ixgbevf_get_stats_count,
#endif
	.get_strings            = ixgbevf_get_strings,
	.get_ethtool_stats      = ixgbevf_get_ethtool_stats,
#ifdef HAVE_ETHTOOL_GET_PERM_ADDR
	.get_perm_addr          = ethtool_op_get_perm_addr,
#endif
	.get_coalesce           = ixgbevf_get_coalesce,
	.set_coalesce           = ixgbevf_set_coalesce,
#ifdef ETHTOOL_COALESCE_USECS
	.supported_coalesce_params = ETHTOOL_COALESCE_USECS,
#endif
#ifdef ETHTOOL_GRXRINGS
	.get_rxnfc		= ixgbevf_get_rxnfc,
	.set_rxnfc		= ixgbevf_set_rxnfc,
#endif
#if defined(HAVE_ETHTOOL_GET_SSET_COUNT) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
	.get_priv_flags		= ixgbevf_get_priv_flags,
	.set_priv_flags		= ixgbevf_set_priv_flags,
#endif /* HAVE_ETHTOOL_GET_SSET_COUNT && HAVE_SWIOTLB_SKIP_CPU_SYNC */
};

#ifdef HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
static const struct ethtool_ops_ext ixgbevf_ethtool_ops_ext = {
	.size		= sizeof(struct ethtool_ops_ext),
	.get_ts_info	= ethtool_op_get_ts_info,
#if defined(ETHTOOL_GRSSH) && defined(ETHTOOL_SRSSH)
	.get_rxfh_indir_size	= ixgbevf_get_rxfh_indir_size,
	.get_rxfh_key_size	= ixgbevf_get_rxfh_key_size,
	.get_rxfh		= ixgbevf_get_rxfh,
#endif /* ETHTOOL_GRSSH && ETHTOOL_SRSSH */
};
#endif /* HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT */

void ixgbevf_set_ethtool_ops(struct net_device *netdev)
{
#ifndef ETHTOOL_OPS_COMPAT
	netdev->ethtool_ops = &ixgbevf_ethtool_ops;
#else
	SET_ETHTOOL_OPS(netdev, &ixgbevf_ethtool_ops);
#endif

#ifdef HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
	set_ethtool_ops_ext(netdev, &ixgbevf_ethtool_ops_ext);
#endif /* HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT */
}
#endif /* SIOCETHTOOL */
