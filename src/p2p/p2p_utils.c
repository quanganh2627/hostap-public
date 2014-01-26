/*
 * P2P - generic helper functions
 * Copyright (c) 2009, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "p2p_i.h"
#include "wps/wps_defs.h"

/**
 * p2p_random - Generate random string for SSID and passphrase
 * @buf: Buffer for returning the result
 * @len: Number of octets to write to the buffer
 * Returns: 0 on success, -1 on failure
 *
 * This function generates a random string using the following character set:
 * 'A'-'Z', 'a'-'z', '0'-'9'.
 */
int p2p_random(char *buf, size_t len)
{
	u8 val;
	size_t i;
	u8 letters = 'Z' - 'A' + 1;
	u8 numbers = 10;

	if (os_get_random((unsigned char *) buf, len))
		return -1;
	/* Character set: 'A'-'Z', 'a'-'z', '0'-'9' */
	for (i = 0; i < len; i++) {
		val = buf[i];
		val %= 2 * letters + numbers;
		if (val < letters)
			buf[i] = 'A' + val;
		else if (val < 2 * letters)
			buf[i] = 'a' + (val - letters);
		else
			buf[i] = '0' + (val - 2 * letters);
	}

	return 0;
}


/**
 * p2p_channel_to_freq - Convert channel info to frequency
 * @op_class: Operating class
 * @channel: Channel number
 * Returns: Frequency in MHz or -1 if the specified channel is unknown
 */
int p2p_channel_to_freq(int op_class, int channel)
{
	/* Table E-4 in IEEE Std 802.11-2012 - Global operating classes */
	/* TODO: more operating classes */
	switch (op_class) {
	case 81:
		/* channels 1..13 */
		if (channel < 1 || channel > 13)
			return -1;
		return 2407 + 5 * channel;
	case 82:
		/* channel 14 */
		if (channel != 14)
			return -1;
		return 2414 + 5 * channel;
	case 83: /* channels 1..9; 40 MHz */
	case 84: /* channels 5..13; 40 MHz */
		if (channel < 1 || channel > 13)
			return -1;
		return 2407 + 5 * channel;
	case 115: /* channels 36,40,44,48; indoor only */
	case 118: /* channels 52,56,60,64; dfs */
		if (channel < 36 || channel > 64)
			return -1;
		return 5000 + 5 * channel;
	case 124: /* channels 149,153,157,161 */
	case 125: /* channels 149,153,157,161,165,169 */
		if (channel < 149 || channel > 161)
			return -1;
		return 5000 + 5 * channel;
	case 116: /* channels 36,44; 40 MHz; indoor only */
	case 117: /* channels 40,48; 40 MHz; indoor only */
	case 119: /* channels 52,60; 40 MHz; dfs */
	case 120: /* channels 56,64; 40 MHz; dfs */
		if (channel < 36 || channel > 64)
			return -1;
		return 5000 + 5 * channel;
	case 126: /* channels 149,157; 40 MHz */
	case 127: /* channels 153,161; 40 MHz */
		if (channel < 149 || channel > 161)
			return -1;
		return 5000 + 5 * channel;
	case 128: /* center freqs 42, 58, 106, 122, 138, 155; 80 MHz */
		if (channel < 36 || channel > 161)
			return -1;
		return 5000 + 5 * channel;
	}
	return -1;
}


/**
 * p2p_freq_to_channel - Convert frequency into channel info
 * @op_class: Buffer for returning operating class
 * @channel: Buffer for returning channel number
 * Returns: 0 on success, -1 if the specified frequency is unknown
 */
int p2p_freq_to_channel(unsigned int freq, u8 *op_class, u8 *channel)
{
	/* TODO: more operating classes */
	if (freq >= 2412 && freq <= 2472) {
		if ((freq - 2407) % 5)
			return -1;

		*op_class = 81; /* 2.407 GHz, channels 1..13 */
		*channel = (freq - 2407) / 5;
		return 0;
	}

	if (freq == 2484) {
		*op_class = 82; /* channel 14 */
		*channel = 14;
		return 0;
	}

	if (freq >= 5180 && freq <= 5240) {
		if ((freq - 5000) % 5)
			return -1;

		*op_class = 115; /* 5 GHz, channels 36..48 */
		*channel = (freq - 5000) / 5;
		return 0;
	}

	if (freq >= 5745 && freq <= 5805) {
		if ((freq - 5000) % 5)
			return -1;

		*op_class = 124; /* 5 GHz, channels 149..161 */
		*channel = (freq - 5000) / 5;
		return 0;
	}

	return -1;
}


static void p2p_reg_class_intersect(const struct p2p_reg_class *a,
				    const struct p2p_reg_class *b,
				    struct p2p_reg_class *res)
{
	size_t i, j;

	res->reg_class = a->reg_class;

	for (i = 0; i < a->channels; i++) {
		for (j = 0; j < b->channels; j++) {
			if (a->channel[i] != b->channel[j])
				continue;
			res->channel[res->channels] = a->channel[i];
			res->channels++;
			if (res->channels == P2P_MAX_REG_CLASS_CHANNELS)
				return;
		}
	}
}


/**
 * p2p_channels_intersect - Intersection of supported channel lists
 * @a: First set of supported channels
 * @b: Second set of supported channels
 * @res: Data structure for returning the intersection of support channels
 *
 * This function can be used to find a common set of supported channels. Both
 * input channels sets are assumed to use the same country code. If different
 * country codes are used, the regulatory class numbers may not be matched
 * correctly and results are undefined.
 */
void p2p_channels_intersect(const struct p2p_channels *a,
			    const struct p2p_channels *b,
			    struct p2p_channels *res)
{
	size_t i, j;

	os_memset(res, 0, sizeof(*res));

	for (i = 0; i < a->reg_classes; i++) {
		const struct p2p_reg_class *a_reg = &a->reg_class[i];
		for (j = 0; j < b->reg_classes; j++) {
			const struct p2p_reg_class *b_reg = &b->reg_class[j];
			if (a_reg->reg_class != b_reg->reg_class)
				continue;
			p2p_reg_class_intersect(
				a_reg, b_reg,
				&res->reg_class[res->reg_classes]);
			if (res->reg_class[res->reg_classes].channels) {
				res->reg_classes++;
				if (res->reg_classes == P2P_MAX_REG_CLASSES)
					return;
			}
		}
	}
}


static void p2p_op_class_union(struct p2p_reg_class *cl,
			       const struct p2p_reg_class *b_cl)
{
	size_t i, j;

	for (i = 0; i < b_cl->channels; i++) {
		for (j = 0; j < cl->channels; j++) {
			if (b_cl->channel[i] == cl->channel[j])
				break;
		}
		if (j == cl->channels) {
			if (cl->channels == P2P_MAX_REG_CLASS_CHANNELS)
				return;
			cl->channel[cl->channels++] = b_cl->channel[i];
		}
	}
}


/**
 * p2p_channels_union - Union of channel lists
 * @a: First set of channels
 * @b: Second set of channels
 * @res: Data structure for returning the union of channels
 */
void p2p_channels_union(const struct p2p_channels *a,
			const struct p2p_channels *b,
			struct p2p_channels *res)
{
	size_t i, j;

	if (a != res)
		os_memcpy(res, a, sizeof(*res));

	for (i = 0; i < res->reg_classes; i++) {
		struct p2p_reg_class *cl = &res->reg_class[i];
		for (j = 0; j < b->reg_classes; j++) {
			const struct p2p_reg_class *b_cl = &b->reg_class[j];
			if (cl->reg_class != b_cl->reg_class)
				continue;
			p2p_op_class_union(cl, b_cl);
		}
	}

	for (j = 0; j < b->reg_classes; j++) {
		const struct p2p_reg_class *b_cl = &b->reg_class[j];

		for (i = 0; i < res->reg_classes; i++) {
			struct p2p_reg_class *cl = &res->reg_class[i];
			if (cl->reg_class == b_cl->reg_class)
				break;
		}

		if (i == res->reg_classes) {
			if (res->reg_classes == P2P_MAX_REG_CLASSES)
				return;
			os_memcpy(&res->reg_class[res->reg_classes++],
				  b_cl, sizeof(struct p2p_reg_class));
		}
	}
}


void p2p_channels_remove_freqs(struct p2p_channels *chan,
			       const struct wpa_freq_range_list *list)
{
	size_t o, c;

	if (list == NULL)
		return;

	o = 0;
	while (o < chan->reg_classes) {
		struct p2p_reg_class *op = &chan->reg_class[o];

		c = 0;
		while (c < op->channels) {
			int freq = p2p_channel_to_freq(op->reg_class,
						       op->channel[c]);
			if (freq > 0 && freq_range_list_includes(list, freq)) {
				op->channels--;
				os_memmove(&op->channel[c],
					   &op->channel[c + 1],
					   op->channels - c);
			} else
				c++;
		}

		if (op->channels == 0) {
			chan->reg_classes--;
			os_memmove(&chan->reg_class[o], &chan->reg_class[o + 1],
				   (chan->reg_classes - o) *
				   sizeof(struct p2p_reg_class));
		} else
			o++;
	}
}


/**
 * p2p_channels_includes - Check whether a channel is included in the list
 * @channels: List of supported channels
 * @reg_class: Regulatory class of the channel to search
 * @channel: Channel number of the channel to search
 * Returns: 1 if channel was found or 0 if not
 */
int p2p_channels_includes(const struct p2p_channels *channels, u8 reg_class,
			  u8 channel)
{
	size_t i, j;
	for (i = 0; i < channels->reg_classes; i++) {
		const struct p2p_reg_class *reg = &channels->reg_class[i];
		if (reg->reg_class != reg_class)
			continue;
		for (j = 0; j < reg->channels; j++) {
			if (reg->channel[j] == channel)
				return 1;
		}
	}
	return 0;
}


int p2p_channels_includes_freq(const struct p2p_channels *channels,
			       unsigned int freq)
{
	size_t i, j;
	for (i = 0; i < channels->reg_classes; i++) {
		const struct p2p_reg_class *reg = &channels->reg_class[i];
		for (j = 0; j < reg->channels; j++) {
			if (p2p_channel_to_freq(reg->reg_class,
						reg->channel[j]) == (int) freq)
				return 1;
		}
	}
	return 0;
}


#ifdef ANDROID_P2P
static int p2p_block_op_freq(unsigned int freq)
{
	return (freq >= 5170 && freq < 5745);
}


static size_t p2p_copy_reg_class(struct p2p_reg_class *dc,
				 struct p2p_reg_class *sc)
{
	unsigned int i;

	dc->reg_class = sc->reg_class;
	dc->channels = 0;
	for (i=0; i < sc->channels; i++) {
		if (!p2p_block_op_freq(p2p_channel_to_freq(sc->reg_class,
							   sc->channel[i]))) {
			dc->channel[dc->channels] = sc->channel[i];
			dc->channels++;
		}
	}
	return dc->channels;
}
#endif


int p2p_supported_freq(struct p2p_data *p2p, unsigned int freq)
{
	u8 op_reg_class, op_channel;

#ifdef ANDROID_P2P
	if (p2p_block_op_freq(freq))
		return 0;
#endif
	if (p2p_freq_to_channel(freq, &op_reg_class, &op_channel) < 0)
		return 0;
	return p2p_channels_includes(&p2p->cfg->channels, op_reg_class,
				     op_channel);
}


int p2p_supported_freq_go(struct p2p_data *p2p, unsigned int freq)
{
	u8 op_reg_class, op_channel;
	if (p2p_freq_to_channel(freq, &op_reg_class, &op_channel) < 0)
		return 0;
	return p2p_channels_includes(&p2p->cfg->channels, op_reg_class,
				     op_channel) &&
		!freq_range_list_includes(&p2p->no_go_freq, freq);
}


int p2p_supported_freq_cli(struct p2p_data *p2p, unsigned int freq)
{
	u8 op_reg_class, op_channel;
	if (p2p_freq_to_channel(freq, &op_reg_class, &op_channel) < 0)
		return 0;
	return p2p_channels_includes(&p2p->cfg->channels, op_reg_class,
				     op_channel) ||
		p2p_channels_includes(&p2p->cfg->cli_channels, op_reg_class,
				      op_channel) ||
		p2p_channels_includes(&p2p->cfg->indoor_channels, op_reg_class,
				      op_channel);
}


unsigned int p2p_get_pref_freq(struct p2p_data *p2p,
			       const struct p2p_channels *channels)
{
	unsigned int i;
	int freq = 0;

	if (channels == NULL) {
		if (p2p->cfg->num_pref_chan) {
			freq = p2p_channel_to_freq(
				p2p->cfg->pref_chan[0].op_class,
				p2p->cfg->pref_chan[0].chan);
			if (freq < 0)
				freq = 0;
		}
		return freq;
	}

	for (i = 0; p2p->cfg->pref_chan && i < p2p->cfg->num_pref_chan; i++) {
		freq = p2p_channel_to_freq(p2p->cfg->pref_chan[i].op_class,
					   p2p->cfg->pref_chan[i].chan);
		if (p2p_channels_includes_freq(channels, freq))
			return freq;
	}

	return 0;
}


void p2p_channels_dump(struct p2p_data *p2p, const char *title,
		       const struct p2p_channels *chan)
{
	char buf[500], *pos, *end;
	size_t i, j;
	int ret;

	pos = buf;
	end = pos + sizeof(buf);

	for (i = 0; i < chan->reg_classes; i++) {
		const struct p2p_reg_class *c;
		c = &chan->reg_class[i];
		ret = os_snprintf(pos, end - pos, " %u:", c->reg_class);
		if (ret < 0 || ret >= end - pos)
			break;
		pos += ret;

		for (j = 0; j < c->channels; j++) {
			ret = os_snprintf(pos, end - pos, "%s%u",
					  j == 0 ? "" : ",",
					  c->channel[j]);
			if (ret < 0 || ret >= end - pos)
				break;
			pos += ret;
		}
	}
	*pos = '\0';

	p2p_dbg(p2p, "%s:%s", title, buf);
}


int p2p_channel_select(struct p2p_channels *chans, const int *classes,
		       u8 *op_class, u8 *op_channel)
{
	unsigned int i, j, r;

	for (j = 0; classes[j]; j++) {
		for (i = 0; i < chans->reg_classes; i++) {
#ifdef ANDROID_P2P
			struct p2p_reg_class prc;
			struct p2p_reg_class *c = &prc;
			p2p_copy_reg_class(c, &chans->reg_class[i]);
#else
			struct p2p_reg_class *c = &chans->reg_class[i];
#endif

			if (c->channels == 0)
				continue;

			if (c->reg_class == classes[j]) {
				/*
				 * Pick one of the available channels in the
				 * operating class at random.
				 */
				os_get_random((u8 *) &r, sizeof(r));
				r %= c->channels;
				*op_class = c->reg_class;
				*op_channel = c->channel[r];
				return 0;
			}
		}
	}

	return -1;
}

unsigned int p2p_is_indoor_device(struct p2p_peer_info *peer)
{
	int cat, sub;
	if (!peer)
		return 0;

	cat = WPA_GET_BE16(peer->pri_dev_type);
	sub = WPA_GET_BE16(&peer->pri_dev_type[6]);

	switch (cat) {
	case WPS_DEV_COMPUTER:
		switch (sub) {
		case WPS_DEV_COMPUTER_SERVER:
		case WPS_DEV_COMPUTER_MEDIA_CENTER:
		case WPS_DEV_COMPUTER_DESKTOP:
			return 1;
		case WPS_DEV_COMPUTER_PC:
		case WPS_DEV_COMPUTER_ULTRA_MOBILE:
		case WPS_DEV_COMPUTER_NOTEBOOK:
		case WPS_DEV_COMPUTER_MID:
		default:
			return 0;
		}
		break;
	case WPS_DEV_MULTIMEDIA:
		switch (sub) {
		case WPS_DEV_MULTIMEDIA_DAR:
		case WPS_DEV_MULTIMEDIA_PVR:
		case WPS_DEV_MULTIMEDIA_MCX:
		case WPS_DEV_MULTIMEDIA_SET_TOP_BOX:
		case WPS_DEV_MULTIMEDIA_MEDIA_SERVER:
			return 1;
		case WPS_DEV_MULTIMEDIA_PORTABLE_VIDEO_PLAYER:
		default:
			return 0;
		}
		break;
	case WPS_DEV_GAMING:
		switch (sub) {
		case WPS_DEV_GAMING_XBOX:
		case WPS_DEV_GAMING_XBOX360:
		case WPS_DEV_GAMING_PLAYSTATION:
		case WPS_DEV_GAMING_GAME_CONSOLE:
			return 1;
		case WPS_DEV_GAMING_PORTABLE_DEVICE:
		default:
			return 0;
		}
		break;
	case WPS_DEV_PRINTER:
	case WPS_DEV_DISPLAY:
	case WPS_DEV_NETWORK_INFRA:
	case WPS_DEV_STORAGE:
		return 1;
	case WPS_DEV_INPUT:
	case WPS_DEV_CAMERA:
	case WPS_DEV_PHONE:
	case WPS_DEV_AUDIO:
	default:
		return 0;
	}
	return 0;
}
