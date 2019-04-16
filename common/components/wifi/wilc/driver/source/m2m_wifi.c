/**
 *
 * \file
 *
 * \brief This module contains M2M Wi-Fi APIs implementation.
 *
 * Copyright (c) 2016 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */

#include "driver/include/m2m_wifi.h"
#include "driver/source/nmdrv.h"
#include "driver/source/m2m_hif.h"
#include "driver/source/nmasic.h"
#if defined(M2M_WILC1000) && defined(COMPUTE_PMK_IN_HOST)
#include "drv_hash/crypto.h" 
#endif

static volatile uint8 gu8ChNum;
static volatile uint8 gu8scanInProgress = 0;
tpfAppWifiCb gpfAppWifiCb = NULL;


#ifdef ETH_MODE
static tpfAppEthCb  gpfAppEthCb  = NULL;
static uint8* 	        gau8ethRcvBuf=NULL;
static uint16 	        gu16ethRcvBufSize ;
#endif

//#define CONF_MGMT

#ifdef CONF_MGMT
static tpfAppMonCb  gpfAppMonCb  = NULL;
static struct _tstrMgmtCtrl
{
	uint8* pu8Buf;
	uint16 u16Offset;
	uint16 u16Sz;
}
gstrMgmtCtrl = {NULL, 0 , 0};
#endif
/**
*	@fn			m2m_wifi_cb(uint8 u8OpCode, uint16 u16DataSize, uint32 u32Addr, uint8 grp)
*	@brief		WiFi call back function
*	@param [in]	u8OpCode
*					HIF Opcode type.
*	@param [in]	u16DataSize
*					HIF data length.
*	@param [in]	u32Addr
*					HIF address.
*	@param [in]	grp
*					HIF group type.
*	@author
*	@date
*	@version	1.0
*/
static void m2m_wifi_cb(uint8 u8OpCode, uint16 u16DataSize, uint32 u32Addr)
{
	uint8 rx_buf[8];
	if (u8OpCode == M2M_WIFI_RESP_CON_STATE_CHANGED)
	{
		tstrM2mWifiStateChanged strState;
		if (hif_receive(u32Addr, (uint8*) &strState,sizeof(tstrM2mWifiStateChanged), 0) == M2M_SUCCESS)
		{
			if (gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_RESP_CON_STATE_CHANGED, &strState);
		}
	}
	else if(u8OpCode == M2M_WIFI_RESP_CONN_INFO)
	{
		tstrM2MConnInfo		strConnInfo;
		if(hif_receive(u32Addr, (uint8*)&strConnInfo, sizeof(tstrM2MConnInfo), 1) == M2M_SUCCESS)
		{
			if(gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_RESP_CONN_INFO, &strConnInfo);
		}
	}
	else if(u8OpCode == M2M_WIFI_RESP_AP_ASSOC_INFO)
	{
		tstrM2MAPAssocInfo	strAssocInfo;
		if(hif_receive(u32Addr, (uint8*)&strAssocInfo, sizeof(tstrM2MAPAssocInfo), 1) == M2M_SUCCESS)
		{
			if(gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_RESP_AP_ASSOC_INFO, &strAssocInfo);
		}
	}
	else if (u8OpCode == M2M_WIFI_RESP_MEMORY_RECOVER)
	{
#if 0
		if (hif_receive(u32Addr, rx_buf, 4, 1) == M2M_SUCCESS)
		{
			tstrM2mWifiStateChanged strState;
			m2m_memcpy((uint8*) &strState, rx_buf,sizeof(tstrM2mWifiStateChanged));
			if (app_wifi_recover_cb)
				app_wifi_recover_cb(strState.u8CurrState);
		}
#endif
	}
	else if (u8OpCode == M2M_WIFI_REQ_DHCP_CONF)
	{
		tstrM2MIPConfig strIpConfig;
		if (hif_receive(u32Addr, (uint8 *)&strIpConfig, sizeof(tstrM2MIPConfig), 0) == M2M_SUCCESS)
		{
			if (gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_REQ_DHCP_CONF, (uint8 *)&strIpConfig.u32StaticIP);
		}
	}
	else if (u8OpCode == M2M_WIFI_REQ_WPS)
	{
		tstrM2MWPSInfo strWps;
		m2m_memset((uint8*)&strWps,0,sizeof(tstrM2MWPSInfo));
		if(hif_receive(u32Addr, (uint8*)&strWps, sizeof(tstrM2MWPSInfo), 0) == M2M_SUCCESS)
		{
			if (gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_REQ_WPS, &strWps);
		}
	}
	else if (u8OpCode == M2M_WIFI_RESP_IP_CONFLICT)
	{
		tstrM2MIPConfig strIpConfig;
		if(hif_receive(u32Addr, (uint8 *)&strIpConfig, sizeof(tstrM2MIPConfig), 0) == M2M_SUCCESS)
		{
			M2M_DBG("Conflicted IP\"%u.%u.%u.%u\"\n", 
				((uint8 *)&strIpConfig.u32StaticIP)[0], ((uint8 *)&strIpConfig.u32StaticIP)[1],
				((uint8 *)&strIpConfig.u32StaticIP)[2], ((uint8 *)&strIpConfig.u32StaticIP)[3]);
			if (gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_RESP_IP_CONFLICT, NULL);

		}
	}
	else if (u8OpCode == M2M_WIFI_RESP_SCAN_DONE)
	{
		tstrM2mScanDone strState;
		gu8scanInProgress = 0;
		if(hif_receive(u32Addr, (uint8*)&strState, sizeof(tstrM2mScanDone), 0) == M2M_SUCCESS)
		{
			gu8ChNum = strState.u8NumofCh;
			if (gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_RESP_SCAN_DONE, &strState);
		}
	}
	else if (u8OpCode == M2M_WIFI_RESP_SCAN_RESULT)
	{
		tstrM2mWifiscanResult strScanResult;
		if(hif_receive(u32Addr, (uint8*)&strScanResult, sizeof(tstrM2mWifiscanResult), 0) == M2M_SUCCESS)
		{
			if (gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_RESP_SCAN_RESULT, &strScanResult);
		}
	}
	else if (u8OpCode == M2M_WIFI_RESP_CURRENT_RSSI)
	{
		if (hif_receive(u32Addr, rx_buf, 4, 0) == M2M_SUCCESS)
		{
			if (gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_RESP_CURRENT_RSSI, rx_buf);
		}
	}
	else if (u8OpCode == M2M_WIFI_RESP_CLIENT_INFO)
	{
		if (hif_receive(u32Addr, rx_buf, 4, 0) == M2M_SUCCESS)
		{
			if (gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_RESP_CLIENT_INFO, rx_buf);
		}
	}
	else if(u8OpCode == M2M_WIFI_RESP_PROVISION_INFO)
	{
		tstrM2MProvisionInfo	strProvInfo;
		if(hif_receive(u32Addr, (uint8*)&strProvInfo, sizeof(tstrM2MProvisionInfo), 1) == M2M_SUCCESS)
		{
			if(gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_RESP_PROVISION_INFO, &strProvInfo);
		}
	}
	else if(u8OpCode == M2M_WIFI_RESP_DEFAULT_CONNECT)
	{
		tstrM2MDefaultConnResp	strResp;
		if(hif_receive(u32Addr, (uint8*)&strResp, sizeof(tstrM2MDefaultConnResp), 1) == M2M_SUCCESS)
		{
			if(gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_RESP_DEFAULT_CONNECT, &strResp);
		}
	}
#ifdef ETH_MODE
	else if(u8OpCode == M2M_WIFI_RESP_ETHERNET_RX_PACKET)
	{
		if(hif_receive(u32Addr, rx_buf ,sizeof(tstrM2mIpRsvdPkt), 0) == M2M_SUCCESS)
		{
			tstrM2mIpRsvdPkt * pstrM2MIpRxPkt = (void *)rx_buf;
			tstrM2mIpCtrlBuf  strM2mIpCtrlBuf;
			uint16 u16Offset = pstrM2MIpRxPkt->u16PktOffset;
			#ifdef M2M_WILC1000
			strM2mIpCtrlBuf.u8IfcId = pstrM2MIpRxPkt->u8IfcId;
			#endif
			
			strM2mIpCtrlBuf.u16RemainigDataSize = pstrM2MIpRxPkt->u16PktSz;
			if((gpfAppEthCb) &&(gau8ethRcvBuf)&& (gu16ethRcvBufSize > 0))
			{
				while (strM2mIpCtrlBuf.u16RemainigDataSize > 0)
				{
					if(strM2mIpCtrlBuf.u16RemainigDataSize > gu16ethRcvBufSize)
					{
						strM2mIpCtrlBuf.u16DataSize = gu16ethRcvBufSize ;
					} 
					else
					{
						strM2mIpCtrlBuf.u16DataSize = strM2mIpCtrlBuf.u16RemainigDataSize;
					}
					if(hif_receive(u32Addr+u16Offset, gau8ethRcvBuf, strM2mIpCtrlBuf.u16DataSize, 0) == M2M_SUCCESS)
					{
						strM2mIpCtrlBuf.u16RemainigDataSize -= strM2mIpCtrlBuf.u16DataSize;
						u16Offset += strM2mIpCtrlBuf.u16DataSize;
						gpfAppEthCb(M2M_WIFI_RESP_ETHERNET_RX_PACKET, gau8ethRcvBuf, &(strM2mIpCtrlBuf));
					}
					else
					{
						break;
					}
				}
			}
		}
	}
	else if(u8OpCode == M2M_WIFI_RESP_PACKET_SENT)
	{
		gpfAppEthCb(M2M_WIFI_RESP_PACKET_SENT, NULL, 0);
	}
#endif	

#ifdef CONF_MGMT
	else if(u8OpCode == M2M_WIFI_RESP_WIFI_RX_PACKET)
	{
		tstrM2MWifiRxPacketInfo		strRxPacketInfo;
		if(hif_receive(u32Addr, (uint8*)&strRxPacketInfo, sizeof(tstrM2MWifiRxPacketInfo), 0) == M2M_SUCCESS)
		{
			u16DataSize -= sizeof(tstrM2MWifiRxPacketInfo);
			if(gstrMgmtCtrl.pu8Buf != NULL && u16DataSize > 0)
			{
				if(u16DataSize > (gstrMgmtCtrl.u16Sz + gstrMgmtCtrl.u16Offset))
				{
					u16DataSize = gstrMgmtCtrl.u16Sz;
				}
				u32Addr += sizeof(tstrM2MWifiRxPacketInfo) + gstrMgmtCtrl.u16Offset;
				if(hif_receive(u32Addr , gstrMgmtCtrl.pu8Buf, u16DataSize, 1) != M2M_SUCCESS)
				{
					u16DataSize = 0;
				}
			}
			if(gpfAppMonCb)
				gpfAppMonCb(&strRxPacketInfo, gstrMgmtCtrl.pu8Buf,u16DataSize);
		}
	}
#endif
	else if(u8OpCode == M2M_WIFI_RESP_FIRMWARE_STRTED)
	{		
		if(gpfAppWifiCb)
				gpfAppWifiCb(M2M_WIFI_RESP_FIRMWARE_STRTED, NULL);
	}
	else
	{
		M2M_ERR("REQ Not defined %d\n",u8OpCode);
	}
}

sint8 m2m_wifi_download_mode()
{
	sint8 ret = M2M_SUCCESS;
	/* Apply device specific initialization. */
	ret = nm_drv_init_download_mode();
	if(ret != M2M_SUCCESS) 	goto _EXIT0;
	

	
	enable_interrupts();

_EXIT0:
	return ret;
}

sint8 m2m_wifi_init(tstrWifiInitParam * param)
{
	sint8 ret = M2M_SUCCESS;

	gpfAppWifiCb = param->pfAppWifiCb;
		
#ifdef ETH_MODE
 	gpfAppEthCb  	    = param->strEthInitParam.pfAppEthCb;
	gau8ethRcvBuf       = param->strEthInitParam.au8ethRcvBuf;
	gu16ethRcvBufSize	= param->strEthInitParam.u16ethRcvBufSize;
#endif

#ifdef CONF_MGMT
	gpfAppMonCb  = param->pfAppMonCb;
#endif
	gu8scanInProgress = 0;
	/* Apply device specific initialization. */
	ret = nm_drv_init(NULL);
	if(ret != M2M_SUCCESS) 	goto _EXIT0;
	/* Initialize host interface module */
	ret = hif_init(NULL);
	if(ret != M2M_SUCCESS) 	goto _EXIT1;

	hif_register_cb(M2M_REQ_GRP_WIFI,m2m_wifi_cb);

	return ret;

_EXIT1:
	nm_drv_deinit(NULL);
_EXIT0:
	return ret;
}

sint8  m2m_wifi_deinit(void * arg)
{

	hif_deinit(NULL);

	nm_drv_deinit(NULL);

	return M2M_SUCCESS;
}


sint8 m2m_wifi_handle_events(void * arg)
{
	return hif_handle_isr();
}

sint8 m2m_wifi_default_connect(void)
{
	return hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_DEFAULT_CONNECT, NULL, 0,NULL, 0,0);
}

sint8 m2m_wifi_connect(char *pcSsid, uint8 u8SsidLen, uint8 u8SecType, void *pvAuthInfo, uint16 u16Ch)
{
	sint8				ret = M2M_SUCCESS;
	tstrM2mWifiConnect	strConnect;
	tstrM2MWifiSecInfo	*pstrAuthInfo;

	if(u8SecType != M2M_WIFI_SEC_OPEN)
	{
		if((pvAuthInfo == NULL)||(m2m_strlen(pvAuthInfo)<=0)||(m2m_strlen(pvAuthInfo)>=M2M_MAX_PSK_LEN))
		{
			M2M_ERR("PSK LEN INVALID\n");
			ret = M2M_ERR_FAIL;
			goto ERR1;
		}
	}
	if((u8SsidLen<=0)||(u8SsidLen>=M2M_MAX_SSID_LEN))
	{
		M2M_ERR("SSID LEN INVALID\n");
		ret = M2M_ERR_FAIL;
		goto ERR1;
	}

	if(u16Ch>M2M_WIFI_CH_14)
	{
		if(u16Ch!=M2M_WIFI_CH_ALL)
		{
			M2M_ERR("CH INVALID\n");
			ret = M2M_ERR_FAIL;
			goto ERR1;
		}
	}


	m2m_memcpy(strConnect.au8SSID, (uint8*)pcSsid, u8SsidLen);
	strConnect.au8SSID[u8SsidLen]	= 0;
	strConnect.u16Ch				= NM_BSP_B_L_16(u16Ch);

	pstrAuthInfo = &strConnect.strSec;
	pstrAuthInfo->u8SecType		= u8SecType;

	if(u8SecType == M2M_WIFI_SEC_WEP)
	{
		tstrM2mWifiWepParams	* pstrWepParams = (tstrM2mWifiWepParams*)pvAuthInfo;
		tstrM2mWifiWepParams	*pstrWep = &pstrAuthInfo->uniAuth.strWepInfo;
		pstrWep->u8KeyIndx =pstrWepParams->u8KeyIndx-1;

		if(pstrWep->u8KeyIndx >= WEP_KEY_MAX_INDEX)
		{
			M2M_ERR("Invalid Wep key index %d\n", pstrWep->u8KeyIndx);
			ret = M2M_ERR_FAIL;
			goto ERR1;
		}
		pstrWep->u8KeySz = pstrWepParams->u8KeySz-1;
		if ((pstrWep->u8KeySz != WEP_40_KEY_STRING_SIZE)&& (pstrWep->u8KeySz != WEP_104_KEY_STRING_SIZE))
		{
			M2M_ERR("Invalid Wep key length %d\n", pstrWep->u8KeySz);
			ret = M2M_ERR_FAIL;
			goto ERR1;
		}
		m2m_memcpy((uint8*)pstrWep->au8WepKey,(uint8*)pstrWepParams->au8WepKey, pstrWepParams->u8KeySz);
		pstrWep->au8WepKey[pstrWepParams->u8KeySz] = 0;

	}


	else if(u8SecType == M2M_WIFI_SEC_WPA_PSK)
	{
		uint8	u8KeyLen = m2m_strlen((uint8*)pvAuthInfo) + 1;
		
#if defined(M2M_WILC1000) && defined(COMPUTE_PMK_IN_HOST)
		pstrAuthInfo->u8IsPMKUsed = 1;
		pbkdf2_sha1((uint8*)pvAuthInfo,u8KeyLen-1,strConnect.au8SSID,m2m_strlen(strConnect.au8SSID),
			pstrAuthInfo->uniAuth.au8PMK);		
#else  
		m2m_memcpy(pstrAuthInfo->uniAuth.au8PSK, (uint8*)pvAuthInfo, u8KeyLen);
#endif
#if defined(M2M_WILC1000) && !defined(COMPUTE_PMK_IN_HOST)
		pstrAuthInfo->u8IsPMKUsed = 0;
#endif

	}
	else if(u8SecType == M2M_WIFI_SEC_802_1X)
	{
		m2m_memcpy((uint8*)&pstrAuthInfo->uniAuth.strCred1x, (uint8*)pvAuthInfo, sizeof(tstr1xAuthCredentials));
	}
	else if(u8SecType == M2M_WIFI_SEC_OPEN)
	{

	}
	else
	{
		M2M_ERR("undefined sec type\n");
		ret = M2M_ERR_FAIL;
		goto ERR1;
	}

	ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_CONNECT, (uint8*)&strConnect, sizeof(tstrM2mWifiConnect),NULL, 0,0);

ERR1:
	return ret;
}

sint8 m2m_wifi_disconnect(void)
{
	return hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_DISCONNECT, NULL, 0, NULL, 0,0);
}

/*!
@fn			NMI_API sint8 m2m_wifi_set_tx_power(uint8 u8TxPwrLevel);
@brief		set the TX power tenuM2mTxPwrLevel
@param [in]	u8TxPwrLevel
			change the TX power tenuM2mTxPwrLevel
@return		The function SHALL return M2M_SUCCESE for success and a negative value otherwise.
@sa			tenuM2mTxPwrLevel
@pre		m2m_wifi_init
@warning	
*/
sint8 m2m_wifi_set_tx_power(uint8 u8TxPwrLevel)
{
	sint8 ret = M2M_SUCCESS;
	tstrM2mTxPwrLevel strM2mTxPwrLevel;
	strM2mTxPwrLevel.u8TxPwrLevel = u8TxPwrLevel;
	ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SET_TX_POWER, (uint8*)&strM2mTxPwrLevel,sizeof(tstrM2mTxPwrLevel), NULL, 0, 0);
	return ret;
}

sint8 m2m_wifi_set_mac_address(uint8 au8MacAddress[6])
{
	tstrM2mSetMacAddress strTmp;
	m2m_memcpy((uint8*) strTmp.au8Mac, (uint8*) au8MacAddress, 6);
	return hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SET_MAC_ADDRESS,
		(uint8*) &strTmp, sizeof(tstrM2mSetMacAddress), NULL, 0,0);
}

sint8 m2m_wifi_set_static_ip(tstrM2MIPConfig * pstrStaticIPConf)
{
	pstrStaticIPConf->u32DNS = NM_BSP_B_L_32(pstrStaticIPConf->u32DNS);
	pstrStaticIPConf->u32Gateway = NM_BSP_B_L_32(pstrStaticIPConf->u32Gateway);
	pstrStaticIPConf->u32StaticIP = NM_BSP_B_L_32(
		pstrStaticIPConf->u32StaticIP);
	pstrStaticIPConf->u32SubnetMask = NM_BSP_B_L_32(
		pstrStaticIPConf->u32SubnetMask);
	return hif_send(M2M_REQ_GRP_IP, M2M_IP_REQ_STATIC_IP_CONF,
		(uint8*) pstrStaticIPConf, sizeof(tstrM2MIPConfig), NULL, 0,0);
}

sint8 m2m_wifi_request_dhcp_client(void)
{
	/*legacy API should be removed */
	return 0;
}
sint8 m2m_wifi_request_dhcp_server(uint8* addr)
{
    /*legacy API should be removed */
	return 0;
}
/*!
@fn			NMI_API sint8 m2m_wifi_set_lsn_int(tstrM2mLsnInt * pstrM2mLsnInt);
@brief		Set the Wi-Fi listen interval for power save operation. It is represented in units
			of AP Beacon periods.
@param [in]	pstrM2mLsnInt
			Structure holding the listen interval configurations.
@return		The function SHALL return 0 for success and a negative value otherwise.
@sa			tstrM2mLsnInt , m2m_wifi_set_sleep_mode
@pre		m2m_wifi_set_sleep_mode shall be called first
@warning	The Function called once after initialization. 
*/
sint8 m2m_wifi_set_lsn_int(tstrM2mLsnInt* pstrM2mLsnInt)
{
	return hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_LSN_INT, (uint8*)pstrM2mLsnInt, sizeof(tstrM2mLsnInt), NULL, 0, 0);
}

sint8 m2m_wifi_set_cust_InfoElement(uint8* pau8M2mCustInfoElement)
{
	return hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_CUST_INFO_ELEMENT, (uint8*)pau8M2mCustInfoElement, pau8M2mCustInfoElement[0]+1, NULL, 0, 0);
}

sint8 m2m_wifi_set_scan_options(uint8 u8NumOfSlot,uint8 u8SlotTime)
{
	sint8	s8Ret = M2M_ERR_FAIL;
	tstrM2MScanOption strM2MScan;
	strM2MScan.u8NumOfSlot = u8NumOfSlot;
	strM2MScan.u8SlotTime = u8SlotTime;

	if(strM2MScan.u8NumOfSlot < M2M_SCAN_MIN_NUM_SLOTS)
	{
		strM2MScan.u8NumOfSlot = M2M_SCAN_MIN_NUM_SLOTS;
	}
	if(strM2MScan.u8SlotTime < M2M_SCAN_MIN_SLOT_TIME)
	{
		strM2MScan.u8SlotTime = M2M_SCAN_MIN_SLOT_TIME;
	}
	s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SET_SCAN_OPTION, (uint8*)&strM2MScan, sizeof(tstrM2MScan),NULL, 0,0);
	if(s8Ret != M2M_SUCCESS)
	{
		M2M_ERR("SCAN Failed Ret = %d\n",s8Ret);
	}
	return s8Ret;
}
sint8 m2m_wifi_set_scan_region(uint8  ScanRegion)
{
	sint8	s8Ret = M2M_ERR_FAIL;
	tstrM2MScanRegion strScanRegion;
	strScanRegion.u8ScanRegion = ScanRegion;
	s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SET_SCAN_REGION, (uint8*)&strScanRegion, sizeof(tstrM2MScanRegion),NULL, 0,0);
	return s8Ret;
}

sint8 m2m_wifi_set_scan_list(tstrM2MScanList* pstrScanList)
{
	sint8	s8Ret = M2M_ERR_FAIL;
	s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SET_SCAN_LIST, (uint8*)pstrScanList, sizeof(tstrM2MScanList),NULL, 0,0);
	if(s8Ret != M2M_SUCCESS)
	{
		M2M_ERR("SCAN List Ret = %d\n",s8Ret);
	}
	return s8Ret;
}

sint8 m2m_wifi_request_scan(uint8 ch)
{
	sint8	s8Ret = M2M_ERR_SCAN_IN_PROGRESS;

	if(!gu8scanInProgress)
	{
		tstrM2MScan strtmp;
		m2m_memset((uint8*)&strtmp,0,sizeof(tstrM2MScan));
		strtmp.u8ChNum = ch;
		s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SCAN, (uint8*)&strtmp, sizeof(tstrM2MScan),NULL, 0,0);

		if(s8Ret == M2M_SUCCESS)
		{
			gu8scanInProgress = 1;
		}
		else
		{
			M2M_ERR("SCAN Failed Ret = %d\n",s8Ret);
		}
	}
	else
	{
		M2M_ERR("SCAN In Progress\n");
	}
	return s8Ret;
}

sint8 m2m_wifi_request_scan_ssid(uint8 ch,char* pcssid)
{
	sint8	s8Ret = M2M_ERR_SCAN_IN_PROGRESS;

	if(!gu8scanInProgress)
	{
		tstrM2MScan strtmp;
		m2m_memset((uint8*)&strtmp,0,sizeof(tstrM2MScan));
		strtmp.u8ChNum = ch;
		if(pcssid)
		{
			uint8 len =	m2m_strlen((uint8 *)pcssid);
			m2m_memcpy(strtmp.au8SSID,(uint8 *)pcssid,len+1);
		}
		s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SCAN, (uint8*)&strtmp, sizeof(tstrM2MScan),NULL, 0,0);

		if(s8Ret == M2M_SUCCESS)
		{
			gu8scanInProgress = 1;
		}
		else
		{
			M2M_ERR("SCAN Failed Ret = %d\n",s8Ret);
		}
	}
	else
	{
		M2M_ERR("SCAN In Progress\n");
	}
	return s8Ret;
}

sint8 m2m_wifi_wps(uint8 u8TriggerType,const char  *pcPinNumber)
{
	tstrM2MWPSConnect strtmp;

	/* Stop Scan if it is ongoing.
	*/
	gu8scanInProgress = 0;
	strtmp.u8TriggerType = u8TriggerType;
	/*If WPS is using PIN METHOD*/
	if (u8TriggerType == WPS_PIN_TRIGGER)
		m2m_memcpy ((uint8*)strtmp.acPinNumber,(uint8*) pcPinNumber,8);
	return hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_WPS, (uint8*)&strtmp,sizeof(tstrM2MWPSConnect), NULL, 0,0);
}
sint8 m2m_wifi_wps_disable(void)
{
	sint8 ret = M2M_SUCCESS;
	ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_DISABLE_WPS, NULL,0, NULL, 0, 0);
	return ret;
}
#ifndef M2M_WILC1000

/*!
@fn			NMI_API sint8 m2m_wifi_req_client_ctrl(uint8 cmd);
@brief		Send a command to the PS Client (An WINC1500 board running the ps_firmware), 
			if the PS client send any commands it will be received in wifi_cb M2M_WIFI_RESP_CLIENT_INFO
@param [in]	cmd
			Control command sent from PS Server to PS Client (command values defined by the application)
@return		The function SHALL return M2M_SUCCESE for success and a negative value otherwise.
@sa			m2m_wifi_req_server_init, M2M_WIFI_RESP_CLIENT_INFO
@pre		m2m_wifi_req_server_init should be called first
@warning	
*/
sint8 m2m_wifi_req_client_ctrl(uint8 u8Cmd)
{

	sint8 ret = M2M_SUCCESS;
#ifdef _PS_SERVER_
	tstrM2Mservercmd	strCmd;
	strCmd.u8cmd = u8Cmd;
	ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_CLIENT_CTRL, (uint8*)&strCmd, sizeof(tstrM2Mservercmd), NULL, 0, 0);
#else
	M2M_ERR("_PS_SERVER_ is not defined\n");
#endif
	return ret;
}
/*!
@fn			NMI_API sint8 m2m_wifi_req_server_init(uint8 ch);
@brief		Initialize the PS Server, The WINC1500 support Non secure communication with another WINC1500, 
			(SERVER/CLIENT) through one byte command (probe request and probe response) without any connection setup
@param [in]	ch
			Server listening channel
@return		The function SHALL return M2M_SUCCESE for success and a negative value otherwise
@sa			m2m_wifi_req_client_ctrl
@warning	The server mode can't be used with any other modes (STA/P2P/AP)
*/
sint8 m2m_wifi_req_server_init(uint8 ch)
{
	sint8 ret = M2M_SUCCESS;
#ifdef _PS_SERVER_
	tstrM2mServerInit strServer;
	strServer.u8Channel = ch;
	ret = hif_send(M2M_REQ_GRP_WIFI,M2M_WIFI_REQ_SERVER_INIT, (uint8*)&strServer, sizeof(tstrM2mServerInit), NULL, 0, 0);
#else
	M2M_ERR("_PS_SERVER_ is not defined\n");
#endif
	return ret;
}
#endif

sint8 m2m_wifi_p2p(uint8 u8Channel)
{
	sint8 ret = M2M_SUCCESS;
	if((u8Channel == 1) || (u8Channel == 6) || (u8Channel == 11))
	{
		tstrM2MP2PConnect strtmp;
		strtmp.u8ListenChannel = u8Channel;
		ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_ENABLE_P2P, (uint8*)&strtmp, sizeof(tstrM2MP2PConnect), NULL, 0,0);
	}
	else
	{
		M2M_ERR("Listen channel should only be 1, 6 or 11\n");
		ret = M2M_ERR_FAIL;
	}
	return ret;
}
sint8 m2m_wifi_p2p_disconnect(void)
{
	sint8 ret = M2M_SUCCESS;
	ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_DISABLE_P2P, NULL, 0, NULL, 0, 0);
	return ret;
}

sint8 m2m_wifi_enable_ap(CONST tstrM2MAPConfig* pstrM2MAPConfig)
{
	sint8 ret = M2M_SUCCESS;
	
		
#if defined(M2M_WILC1000) && defined(COMPUTE_PMK_IN_HOST)
	if(pstrM2MAPConfig->u8SecType == M2M_WIFI_SEC_WPA_PSK)
	{
		tstrM2MAPConfig strTempM2MAPConfig;
		m2m_memcpy((uint8 *)&strTempM2MAPConfig, (uint8 *)pstrM2MAPConfig, sizeof(tstrM2MAPConfig));
			
		strTempM2MAPConfig.u8IsPMKUsed = 1;
		pbkdf2_sha1((uint8 *)pstrM2MAPConfig->au8PSK,m2m_strlen((uint8 *)pstrM2MAPConfig->au8PSK),
			(uint8 *)pstrM2MAPConfig->au8SSID,m2m_strlen((uint8 *)pstrM2MAPConfig->au8SSID),strTempM2MAPConfig.au8PMK);
		pstrM2MAPConfig = &strTempM2MAPConfig;
	}
#elif defined(M2M_WILC1000) && !defined(COMPUTE_PMK_IN_HOST)
	if(pstrM2MAPConfig->u8SecType == M2M_WIFI_SEC_WPA_PSK)
	{
		tstrM2MAPConfig strTempM2MAPConfig;
		strTempM2MAPConfig.u8IsPMKUsed = 0;
		m2m_memcpy((uint8*)&strTempM2MAPConfig,(uint8*)pstrM2MAPConfig,sizeof(tstrM2MAPConfig));
		pstrM2MAPConfig = &strTempM2MAPConfig;
	}
#endif
	ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_ENABLE_AP, (uint8 *)pstrM2MAPConfig, sizeof(tstrM2MAPConfig), NULL, 0, 0);	
	
	return ret;
}
sint8 m2m_wifi_disable_ap(void)
{
	sint8 ret = M2M_SUCCESS;
	ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_DISABLE_AP, NULL, 0, NULL, 0, 0);
	return ret;
}
sint8 m2m_wifi_ap_get_assoc_info(void)
{
	sint8 ret = M2M_SUCCESS;
	ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_AP_ASSOC_INFO, NULL, 0, NULL, 0, 0);
	return ret;
}
sint8 m2m_wifi_ap_add_black_list(uint8 bAddNewEntry,uint8* mac_addr)
{
	uint8 u8BlackListRequest[7];

	u8BlackListRequest[0] = bAddNewEntry;
	m2m_memcpy(&u8BlackListRequest[1],mac_addr,6);
	sint8 ret = M2M_SUCCESS;
	ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_AP_BLACK_LIST,u8BlackListRequest, sizeof(u8BlackListRequest), NULL, 0, 0);
	return ret;
}


/*!
@fn          NMI_API sint8 m2m_wifi_req_curr_rssi(void);
@brief       Request the current RSSI for the current connected AP, 
			 the response received in wifi_cb M2M_WIFI_RESP_CURRENT_RSSI	
@sa          M2M_WIFI_RESP_CURRENT_RSSI              
@return      The function shall return M2M_SUCCESS for success and a negative value otherwise.
*/
sint8 m2m_wifi_req_curr_rssi(void)
{
	sint8 ret = M2M_SUCCESS;
	ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_CURRENT_RSSI, NULL, 0, NULL,0, 0);
	return ret;
}
sint8 m2m_wifi_send_ethernet_pkt(uint8* pu8Packet,uint16 u16PacketSize)
{
	sint8	s8Ret = -1;
	if((pu8Packet != NULL)&&(u16PacketSize>0))
	{
		tstrM2MWifiTxPacketInfo		strTxPkt;

		strTxPkt.u16PacketSize		= u16PacketSize;
		strTxPkt.u16HeaderLength	= M2M_ETHERNET_HDR_LEN;
#ifdef M2M_WILC1000
		strTxPkt.u8IfcId			= INTERFACE_1;
#endif
		s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SEND_ETHERNET_PACKET | M2M_REQ_DATA_PKT,
		(uint8*)&strTxPkt, sizeof(tstrM2MWifiTxPacketInfo), pu8Packet, u16PacketSize,  M2M_ETHERNET_HDR_OFFSET - M2M_HIF_HDR_OFFSET);
	}
	return s8Ret;
}
/*!
@fn          NMI_API sint8 m2m_wifi_get_otp_mac_address(uint8 *pu8MacAddr, uint8 * pu8IsValid);
@brief       Request the MAC address stored on the OTP (one time programmable) memory of the device.
			 (the function is Blocking until response received)	
@param [out] pu8MacAddr
			 Output MAC address buffer of 6 bytes size. Valid only if *pu8Valid=1.
@param [out] pu8IsValid
		     A output boolean value to indicate the validity of pu8MacAddr in OTP. 
		     Output zero if the OTP memory is not programmed, non-zero otherwise.	
@return      The function shall return M2M_SUCCESS for success and a negative value otherwise.
@sa          m2m_wifi_get_mac_address             
@pre         m2m_wifi_init required to call any WIFI/socket function
*/
sint8 m2m_wifi_get_otp_mac_address(uint8 *pu8MacAddr, uint8* pu8IsValid)
{
	sint8 ret = M2M_SUCCESS;
	ret = hif_chip_wake();
	if(ret == M2M_SUCCESS)
	{
		ret = nmi_get_otp_mac_address(pu8MacAddr, pu8IsValid);
		if(ret == M2M_SUCCESS)
		{
			ret = hif_chip_sleep();
		}
	}
	return ret;
}
/*!
@fn          NMI_API sint8 m2m_wifi_get_mac_address(uint8 *pu8MacAddr)
@brief       Request the current MAC address of the device (the working mac address).
			 (the function is Blocking until response received)	
@param [out] pu8MacAddr
			 Output MAC address buffer of 6 bytes size.	
@return      The function shall return M2M_SUCCESS for success and a negative value otherwise.
@sa          m2m_wifi_get_otp_mac_address             
@pre         m2m_wifi_init required to call any WIFI/socket function
*/
sint8 m2m_wifi_get_mac_address(uint8 *pu8MacAddr)
{
	sint8 ret = M2M_SUCCESS;
	ret = hif_chip_wake();
	if(ret == M2M_SUCCESS)
	{
		ret = nmi_get_mac_address(pu8MacAddr);
		if(ret == M2M_SUCCESS)
		{
			ret = hif_chip_sleep();
		}
	}

	return ret;
}
/*!
@fn          NMI_API sint8 m2m_wifi_req_scan_result(uint8 index);
@brief       Reads the AP information from the Scan Result list with the given index, 
			 the response received in wifi_cb M2M_WIFI_RESP_SCAN_RESULT, 
			 the response pointer should be casted with tstrM2mWifiscanResult structure 	
@param [in]  index 
			 Index for the requested result, the index range start from 0 till number of AP's found 
@sa          tstrM2mWifiscanResult,m2m_wifi_get_num_ap_found,m2m_wifi_request_scan             
@return      The function shall return M2M_SUCCESE for success and a negative value otherwise
@pre         m2m_wifi_request_scan need to be called first, then m2m_wifi_get_num_ap_found 
			 to get the number of AP's found
@warning     Function used only in STA mode only. the scan result updated only if scan request called,
			 else it will be cashed in firmware for the host scan request result, 
			 which mean if large delay occur between the scan request and the scan result request, 
			 the result will not be up-to-date
*/

sint8 m2m_wifi_req_scan_result(uint8 index)
{
	sint8 ret = M2M_SUCCESS;
	tstrM2mReqScanResult strReqScanRlt;
	strReqScanRlt.u8Index = index;
	ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SCAN_RESULT, (uint8*) &strReqScanRlt, sizeof(tstrM2mReqScanResult), NULL, 0, 0);
	return ret;
}
/*!
@fn          NMI_API uint8 m2m_wifi_get_num_ap_found(void);
@brief       Reads the number of AP's found in the last Scan Request, 
			 The function read the number of AP's from global variable which updated in the 
			 wifi_cb in M2M_WIFI_RESP_SCAN_DONE.			 
@sa          m2m_wifi_request_scan               
@return      Return the number of AP's found in the last Scan Request.
@pre         m2m_wifi_request_scan need to be called first 
@warning     That function need to be called in the wifi_cb in M2M_WIFI_RESP_SCAN_DONE, 
			 calling that function in any other place will return undefined/undated numbers.
			 Function used only in STA mode only.
*/
uint8 m2m_wifi_get_num_ap_found(void)
{
	return gu8ChNum;
}
/*!
@fn		    NMI_API uint8 m2m_wifi_get_sleep_mode(void);
@brief	    Get the current Power save mode.
@return	    The current operating power saving mode.
@sa		    tenuPowerSaveModes , m2m_wifi_set_sleep_mode
*/
uint8 m2m_wifi_get_sleep_mode(void)
{
	return hif_get_sleep_mode();
}
/*!
@fn			NMI_API sint8 m2m_wifi_set_sleep_mode(uint8 PsTyp, uint8 BcastEn);
@brief      Set the power saving mode for the WINC1500. 
@param [in]	PsTyp
			Desired power saving mode. Supported types are defined in tenuPowerSaveModes.
@param [in]	BcastEn
			Broadcast reception enable flag. 
			If it is 1, the WINC1500 must be awake each DTIM Beacon for receiving Broadcast traffic.
			If it is 0, the WINC1500 will not wakeup at the DTIM Beacon, but its wakeup depends only 
			on the the configured Listen Interval. 
@return     The function SHALL return 0 for success and a negative value otherwise.
@sa			tenuPowerSaveModes
@warning    The function called once after initialization.  
*/
sint8 m2m_wifi_set_sleep_mode(uint8 PsTyp, uint8 BcastEn)
{
	sint8 ret = M2M_SUCCESS;
	tstrM2mPsType strPs;
	strPs.u8PsType = PsTyp;
	strPs.u8BcastEn = BcastEn;
	ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SLEEP, (uint8*) &strPs,sizeof(tstrM2mPsType), NULL, 0, 0);
	M2M_INFO("POWER SAVE %d\n",PsTyp);
	hif_set_sleep_mode(PsTyp);
	return ret;
}
/*!
@fn	        NMI_API sint8 m2m_wifi_request_sleep(void)
@brief	    Request from WINC1500 device to Sleep for specific time in the M2M_PS_MANUAL Power save mode (only).
@param [in]	u32SlpReqTime
			Request Sleep in ms 
@return     The function SHALL return M2M_SUCCESS for success and a negative value otherwise.
@sa         tenuPowerSaveModes , m2m_wifi_set_sleep_mode
@warning	the Function should be called in M2M_PS_MANUAL power save only 
*/
sint8 m2m_wifi_request_sleep(uint32 u32SlpReqTime)
{
	sint8 ret = M2M_SUCCESS;
	uint8 psType;
	psType = hif_get_sleep_mode();
	if(psType == M2M_PS_MANUAL)
	{
		tstrM2mSlpReqTime strPs;
		strPs.u32SleepTime = u32SlpReqTime;
		ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_DOZE, (uint8*) &strPs,sizeof(tstrM2mSlpReqTime), NULL, 0, 0);
	}
	ret = hif_chip_sleep();
	return ret;
}
/*!
@fn			NMI_API sint8 m2m_wifi_set_device_name(uint8 *pu8DeviceName, uint8 u8DeviceNameLength);
@brief		Set the WINC1500 device name which is used as P2P device name.
@param [in]	pu8DeviceName
			Buffer holding the device name.
@param [in]	u8DeviceNameLength
			Length of the device name.
@return		The function SHALL return M2M_SUCCESS for success and a negative value otherwise.
@warning	The Function called once after initialization. 
*/
sint8 m2m_wifi_set_device_name(uint8 *pu8DeviceName, uint8 u8DeviceNameLength)
{
	tstrM2MDeviceNameConfig strDeviceName;
	if(u8DeviceNameLength >= M2M_DEVICE_NAME_MAX)
	{
		u8DeviceNameLength = M2M_DEVICE_NAME_MAX;
	}
	//pu8DeviceName[u8DeviceNameLength] = '\0';
	u8DeviceNameLength ++;
	m2m_memcpy(strDeviceName.au8DeviceName, pu8DeviceName, u8DeviceNameLength);
	return hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SET_DEVICE_NAME,
		(uint8*)&strDeviceName, sizeof(tstrM2MDeviceNameConfig), NULL, 0,0);
}
#ifdef CONF_MGMT
sint8 m2m_wifi_enable_monitoring_mode(tstrM2MWifiMonitorModeCtrl *pstrMtrCtrl, uint8 *pu8PayloadBuffer,
								   uint16 u16BufferSize, uint16 u16DataOffset)
{
	sint8	s8Ret = -1;
	if((pstrMtrCtrl->u8ChannelID >= M2M_WIFI_CH_1) && (pstrMtrCtrl->u8ChannelID <= M2M_WIFI_CH_14))
	{
		gstrMgmtCtrl.pu8Buf		= pu8PayloadBuffer;
		gstrMgmtCtrl.u16Sz		= u16BufferSize;
		gstrMgmtCtrl.u16Offset	= u16DataOffset;
		s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_ENABLE_MONITORING | M2M_REQ_DATA_PKT,
			(uint8*)pstrMtrCtrl, sizeof(tstrM2MWifiMonitorModeCtrl), NULL, 0,0);
	}
	return s8Ret;
}

sint8 m2m_wifi_disable_monitoring_mode(void)
{
	return hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_DISABLE_MONITORING, NULL, 0, NULL, 0,0);
}

sint8 m2m_wifi_change_monitoring_channel(uint8 u8ChannelID)
{
	sint8	s8Ret = -1;
	if((u8ChannelID >= M2M_WIFI_CH_1) && (u8ChannelID <= M2M_WIFI_CH_14))
	{		
		s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_CHG_MONITORING_CHNL | M2M_REQ_DATA_PKT,
			(uint8*)&u8ChannelID, sizeof(u8ChannelID), NULL, 0,0);
	}
	return s8Ret;
}

sint8 m2m_wifi_send_wlan_pkt(uint8 *pu8WlanPacket, uint16 u16WlanHeaderLength, uint16 u16WlanPktSize)
{
	sint8	s8Ret = -1;
	if(pu8WlanPacket != NULL)
	{
		tstrM2MWifiTxPacketInfo		strTxPkt;

		strTxPkt.u16PacketSize		= u16WlanPktSize;
		strTxPkt.u16HeaderLength	= u16WlanHeaderLength;
		s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SEND_WIFI_PACKET | M2M_REQ_DATA_PKT,
		(uint8*)&strTxPkt, sizeof(tstrM2MWifiTxPacketInfo), pu8WlanPacket, u16WlanPktSize, sizeof(tstrM2MWifiTxPacketInfo));
	}
	return s8Ret;
}
#endif
#ifndef M2M_WILC1000
sint8 m2m_wifi_start_provision_mode(tstrM2MAPConfig *pstrAPConfig, char *pcHttpServerDomainName, uint8 bEnableHttpRedirect)
{
	sint8	s8Ret = M2M_ERR_FAIL;

	if((pstrAPConfig != NULL))
	{
		tstrM2MProvisionModeConfig	strProvConfig;
		m2m_memcpy((uint8*)&strProvConfig.strApConfig, (uint8*)pstrAPConfig, sizeof(tstrM2MAPConfig));
		m2m_memcpy((uint8*)strProvConfig.acHttpServerDomainName, (uint8*)pcHttpServerDomainName, 64);
		strProvConfig.u8EnableRedirect = bEnableHttpRedirect;
		
		/* Stop Scan if it is ongoing.
		*/
		gu8scanInProgress = 0;
		s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_START_PROVISION_MODE | M2M_REQ_DATA_PKT, (uint8*)&strProvConfig, sizeof(tstrM2MProvisionModeConfig),
		NULL, 0, 0);
	}
	return s8Ret;
}

sint8 m2m_wifi_stop_provision_mode(void)
{
	return hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_STOP_PROVISION_MODE, NULL, 0, NULL, 0, 0);
}
#endif
sint8 m2m_wifi_get_connection_info(void)
{
	return hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_GET_CONN_INFO, NULL, 0, NULL, 0, 0);
}

sint8 m2m_wifi_set_sytem_time(uint32 u32UTCSeconds)
{
	return hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SET_SYS_TIME, (uint8*)&u32UTCSeconds, sizeof(tstrSystemTime), NULL, 0, 0);
}
#ifndef M2M_WILC1000

sint8 m2m_wifi_enable_sntp(uint8 bEnable)
{
	uint8	u8Req;

	u8Req = bEnable ? M2M_WIFI_REQ_ENABLE_SNTP_CLIENT : M2M_WIFI_REQ_DISABLE_SNTP_CLIENT;
	return hif_send(M2M_REQ_GRP_WIFI, u8Req, NULL, 0, NULL, 0, 0);
}
#endif
#ifdef ETH_MODE
/*!
@fn	\
	 NMI_API sint8 m2m_wifi_enable_mac_mcast(uint8* pu8MulticastMacAddress, uint8 u8AddRemove)

@brief
	Add MAC filter to receive Multicast packets.

@param [in]	pu8MulticastMacAddress
				Pointer to the MAC address.
@param [in] u8AddRemove
				Flag to Add/Remove MAC address.
@return		
	The function SHALL return 0 for success and a negative value otherwise.
*/

NMI_API sint8 m2m_wifi_enable_mac_mcast(uint8* pu8MulticastMacAddress, uint8 u8AddRemove)
{
	sint8 s8ret = M2M_ERR_FAIL;
	tstrM2MMulticastMac  strMulticastMac;

	if(pu8MulticastMacAddress != NULL )
	{
		strMulticastMac.u8AddRemove = u8AddRemove;
		m2m_memcpy(strMulticastMac.au8macaddress,pu8MulticastMacAddress,M2M_MAC_ADDRES_LEN);
		M2M_DBG("mac multicast: %x:%x:%x:%x:%x:%x\r\n",strMulticastMac.au8macaddress[0],strMulticastMac.au8macaddress[1],strMulticastMac.au8macaddress[2],strMulticastMac.au8macaddress[3],strMulticastMac.au8macaddress[4],strMulticastMac.au8macaddress[5]);
		s8ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SET_MAC_MCAST, (uint8 *)&strMulticastMac,sizeof(tstrM2MMulticastMac),NULL,0,0);
	}

	return s8ret;

}

/*!
@fn	\
	NMI_API sint8  m2m_wifi_set_receive_buffer(void* pvBuffer,uint16 u16BufferLen);

@brief
	set the ethernet receive buffer, should be called in the receive call back.

@param [in]	pvBuffer
				Pointer to the ethernet receive buffer.
@param [in] u16BufferLen
				Length of the buffer.

@return		
	The function SHALL return 0 for success and a negative value otherwise.
*/
NMI_API sint8  m2m_wifi_set_receive_buffer(void* pvBuffer,uint16 u16BufferLen)
{
	sint8 s8ret = M2M_SUCCESS;
	if(pvBuffer != NULL)
	{
		gau8ethRcvBuf = pvBuffer;
		gu16ethRcvBufSize= u16BufferLen;
	}
	else
	{
		s8ret = M2M_ERR_FAIL;
		M2M_ERR("Buffer NULL pointer\r\n");
	}
	return s8ret;
}
#endif
#if defined(M2M_WILC1000)
sint8 m2m_wifi_download_cert(uint8* pCertData,uint32 u32CertSize)
{
	sint8	s8Ret = -1;
	char pChunk[32];
	uint32 u32ChunkSize = 32,u32TempSize,u32OrigCertSize = u32CertSize;
	if(u32CertSize > M2M_ENTERPRISE_CERT_MAX_LENGTH_IN_BYTES)
	{
		M2M_ERR("too large Cert. file, Max is %d\n",M2M_ENTERPRISE_CERT_MAX_LENGTH_IN_BYTES);
		return s8Ret;
	}
	while(u32CertSize > 0)
	{
		if(u32CertSize < u32ChunkSize)
			u32ChunkSize = u32CertSize;

		u32TempSize =  (u32OrigCertSize&0xffff)|(u32ChunkSize<<16);
		m2m_memcpy((uint8 *)pChunk, &pCertData[u32OrigCertSize-u32CertSize], u32ChunkSize);
		s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_CERT_ADD_CHUNK, (uint8*)&u32TempSize, sizeof(u32CertSize),
				(uint8 *)pChunk, u32ChunkSize,sizeof(u32CertSize));
		u32CertSize -= u32ChunkSize;
	}
	return hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_CERT_DOWNLOAD_DONE, NULL, 0, NULL, 0, 0);	
}
#endif
#if defined(M2M_WILC1000) && defined(CONCURRENT_INTERFACES)
sint8 m2m_wifi_set_control_ifc(uint8 u8IfcId)
{
	sint8	s8Ret = -1;
	tstrM2MIfId strIfId;
	strIfId.u8IfcId=u8IfcId;
	if(u8IfcId == INTERFACE_1 || u8IfcId == INTERFACE_2)
	{		
		s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SET_IFC_ID, (uint8*)&strIfId, sizeof(tstrM2MIfId), NULL, 0, 0);
	}
	return s8Ret;
}

sint8 m2m_wifi_send_ethernet_pkt_ifc1(uint8* pu8Packet,uint16 u16PacketSize)	
{
	sint8	s8Ret = -1;
	if((pu8Packet != NULL)&&(u16PacketSize>0))
	{
		tstrM2MWifiTxPacketInfo		strTxPkt;

		strTxPkt.u16PacketSize		= u16PacketSize;
		strTxPkt.u16HeaderLength	= M2M_ETHERNET_HDR_LEN;
		strTxPkt.u8IfcId			= INTERFACE_2;
		s8Ret = hif_send(M2M_REQ_GRP_WIFI, M2M_WIFI_REQ_SEND_ETHERNET_PACKET | M2M_REQ_DATA_PKT,
		(uint8*)&strTxPkt, sizeof(tstrM2MWifiTxPacketInfo), pu8Packet, u16PacketSize,  M2M_ETHERNET_HDR_OFFSET - M2M_HIF_HDR_OFFSET);
	}
	return s8Ret;
}
#endif
