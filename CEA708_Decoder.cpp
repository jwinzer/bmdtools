/*
 * CEA708_Decoder.cpp
 *
 *  Created on: May 17, 2017
 *      Author: Jochen Winzer
 */

#include <stdio.h>
#include "CEA708_Decoder.h"

#ifdef DEBUG
#define D if (1)
#else
#define D if (0)
#endif

namespace CEA708 {

Decoder::Decoder(uint32_t* vancData, uint32_t vancByteCount) :
		m_vancData(vancData),
		m_vancByteCount(vancByteCount),
		m_vancDataStream(vancData),
		m_vancDataPosition(0),
		m_cdpSize(0),
		m_cdpData(NULL)
{
}

Decoder::~Decoder()
{
}

uint32_t Decoder::GetDataWord()
{
	switch (m_vancDataPosition++ % 3)
	{
	case 0:
		return (*m_vancDataStream++ >> 10) & 0x3ff;

	case 1:
		return *m_vancDataStream & 0x3ff;

	case 2:
		return (*m_vancDataStream++ >> 20) & 0x3ff;

	default:
		return 0;
	}
}

bool Decoder::GetDataPacket(uint32_t size)
{
	if (m_vancDataPosition + size > m_vancByteCount) return false;

	for (unsigned i = 0; i < size; i++)
	{
		switch (m_vancDataPosition++ % 3)
		{
		case 0:
			m_cdp[i] = (*m_vancDataStream++ >> 10) & 0xff;
			break;

		case 1:
			m_cdp[i] = *m_vancDataStream & 0xff;
			break;

		case 2:
			m_cdp[i] = (*m_vancDataStream++ >> 20) & 0xff;
			break;
		}
	}
	return true;
}

uint16_t Decoder::Get608Captions()
{
	const uint8_t	CC_608_FIELD1 = 0xFC;
	const uint8_t	CC_608_FIELD2 = 0xFD;

	uint8_t* ccData = m_cdpData + 1;
	uint32_t count = m_cdpData[0] & 0x1f;
	uint16_t ccWord = 0;
	uint32_t found = 0;

	for (unsigned i = 0; i < count; i++)
	{
		if (*ccData++ == CC_608_FIELD1)
		{
			ccWord = (uint16_t)ccData[1] << 8 | (uint16_t)ccData[0];
			found++;
			D printf(" Captions: %02x %02x\n", ccData[0] & 0x7f, ccData[1] & 0x7f);
			break;
		}
		ccData += 2;
	}
	if (!found) D printf(" No captions.\n");

	return ccWord;
}

bool Decoder::GetCaptionDistributionPacket()
{
	static const uint32_t 	ADF1 = 0x000;
	static const uint32_t 	ADF2 = 0x3ff;
	static const uint32_t 	ADF3 = 0x3ff;
	static const uint32_t 	DID708 = 0x161;
	static const uint32_t 	SDID708 = 0x101;

	static const uint16_t 	CDP_ID = 0x9669;
	static const uint8_t	CDP_DATA_HEADER = 0x72;

	enum
	{
		kCDPHeaderLength = 7,
		kTimeCodeLength = 5,
		kServiceInfoLength = 9,
		kCDPFooterLength = 4
	};

	enum cdp_flags
	{
		time_code_present = 1 << 7,
		ccdata_present = 1 << 6,
		svcinfo_present = 1 << 5,
		svc_info_start = 1 << 4,
		svc_info_change = 1 << 3,
		svc_info_complete = 1 << 2,
		caption_service_active = 1 << 1
	};

	enum cdp_state
	{
		cdp_start = 0,
		cdp_adf1,
		cdp_adf2,
		cdp_adf3,
		cdp_did,
		cdp_sdid,
		cdp_cs
	} state = cdp_start;

	uint32_t did;
	uint32_t sdid;
	uint32_t dc;
	uint32_t checkSum;

	while (m_vancDataPosition < m_vancByteCount)
	{
		uint32_t dw = GetDataWord();
		D printf("state: %d\n", state);

		switch (state)
		{
		case cdp_start:
			if (dw == ADF1)
			{
				state = cdp_adf1;
				break;
			}
			return false;	// give up

		case cdp_adf1:
			state = (dw == ADF2) ? cdp_adf2 : cdp_start;
			break;

		case cdp_adf2:
			state = (dw == ADF3) ? cdp_adf3 : cdp_start;
			break;

		case cdp_adf3:
			did = dw;
			state = cdp_did;
			break;

		case cdp_did:
			sdid = dw;
			state = cdp_sdid;
			break;

		case cdp_sdid:
			dc = dw & 0xff;
			if (GetDataPacket(dc))
			{
				state = cdp_cs;
				break;
			}
			D printf("Error: incomplete packet.\n");
			return false;

		case cdp_cs:
			// TODO: validate checksum
			checkSum = dw & 0xff;

			// Validate caption distribution packet.
			if (did == DID708 && sdid == SDID708)
			{
				uint8_t* cdp_header = m_cdp;
				uint8_t* cdp_data = m_cdp + kCDPHeaderLength;
				if (cdp_header[4] & time_code_present)
					cdp_data += kTimeCodeLength;

				if (cdp_header[0] == (CDP_ID >> 8) &&
						cdp_header[1] == (CDP_ID & 0xff) &&
						*cdp_data++ == CDP_DATA_HEADER)
				{
					m_cdpSize = dc;
					m_cdpData = cdp_data;
					D printf("CDP received, size: %d\n", m_cdpSize);
					return true;
				}
			}
			// Keep parsing.
			state = cdp_start;
			D printf("Error: invalid data packet.\n");
			break;

		default:
			return false;
		}
	}
	return false;
}

} /* namespace CEA708 */
