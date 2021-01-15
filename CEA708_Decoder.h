/*
 * CEA708_Decoder.h
 *
 *  Created on: May 17, 2017
 *      Author: Jochen Winzer
 */

#ifndef CEA708_DECODER_H_
#define CEA708_DECODER_H_

#include <stdint.h>

namespace CEA708 {

class Decoder {
private:
	uint32_t* 	m_vancData;
	uint32_t	m_vancByteCount;

	uint32_t*	m_vancDataStream;
	uint32_t	m_vancDataPosition;

public:
	uint8_t		m_cdp[5120];	// ((1920 * 47) / 48) * 128
	uint32_t	m_cdpSize;
	uint8_t*	m_cdpData;

private:
	uint32_t GetDataWord();
	bool GetDataPacket(uint32_t size);

public:
	Decoder(uint32_t* vancData, uint32_t vancByteCount );
	virtual ~Decoder();

	uint16_t Get608Captions();
	bool GetCaptionDistributionPacket();
};

} /* namespace CEA708 */
#endif /* CEA708_DECODER_H_ */

