int indexOf_shift (const char* base, char* str, int startIndex) {
    int result;
    int baselen = strlen(base);
    // str should not longer than base
    if (strlen(str) > baselen || startIndex > baselen) {
        result = -1;
    } else {
        if (startIndex < 0 ) {
            startIndex = 0;
        }
        char* pos = strstr(base+startIndex, str);
        if (pos == NULL) {
            result = -1;
        } else {
            result = pos - base;
        }
    }
    return result;
}
int indexOf (const char* base, char* str) {
    return indexOf_shift(base, str, 0);
}
/** use two index to search in two part to prevent the worst case
 * (assume search 'aaa' in 'aaaaaaaa', you cannot skip three char each time)
 */
int lastIndexOf (const char* base, char* str) {
    int result;
    // str should not longer than base
    if (strlen(str) > strlen(base)) {
        result = -1;
    } else {
        int start = 0;
        int endinit = strlen(base) - strlen(str);
        int end = endinit;
        int endtmp = endinit;
        while(start != end) {
            start = indexOf_shift(base, str, start);
            end = indexOf_shift(base, str, end);

            // not found from start
            if (start == -1) {
                end = -1; // then break;
            } else if (end == -1) {
                // found from start
                // but not found from end
                // move end to middle
                if (endtmp == (start+1)) {
                    end = start; // then break;
                } else {
                    end = endtmp - (endtmp - start) / 2;
                    if (end <= start) {
                        end = start+1;
                    }
                    endtmp = end;
                }
            } else {
                // found from both start and end
                // move start to end and
                // move end to base - strlen(str)
                start = end;
                end = endinit;
            }
        }
        result = start;
    }
    return result;
}

char* substring(const char *s, int start, int end) {
    char* str = NULL;
    strncpy(str, s+start, end-start);
    return str;
}

// if s1 starts with s2 returns true, else false
// len is the length of s1
// s2 should be null-terminated
static bool starts_with(const char *s1, int len, const char *s2)
{
	int n = 0;
	while (*s2 && n < len) {
		if (*s1++ != *s2++)
			return false;
		n++;
	}
	return *s2 == 0;
}

// convert escape sequence to event, and return consumed bytes on success (failure == 0)
static int parse_escape_seq(struct tb_event *event, const char *buf, int len)
{
	if (len >= 6 && starts_with(buf, len, "\033[M")) {

        char b = buf[3] - 32;
		switch (b & 3) {
		case 0:
			if ((b & 64) != 0)
				event->key = TB_KEY_MOUSE_WHEEL_UP;
			else
				event->key = TB_KEY_MOUSE_LEFT;
			break;
		case 1:
			if ((b & 64) != 0)
				event->key = TB_KEY_MOUSE_WHEEL_DOWN;
			else
				event->key = TB_KEY_MOUSE_MIDDLE;
			break;
		case 2:
			event->key = TB_KEY_MOUSE_RIGHT;
			break;
		case 3:
			event->key = TB_KEY_MOUSE_RELEASE;
			break;
		default:
			return -6;
		}
		event->type = TB_EVENT_MOUSE; // TB_EVENT_KEY by default
        if ((b & 32) != 0) {
            event->mod |= TB_MOD_MOTION;
        }

		// the coord is 1,1 for upper left
		event->x = (uint8_t)buf[4] - 1 - 32;
		event->y = (uint8_t)buf[5] - 1 - 32;

        return 6;
    } else if ((len >= 6 && starts_with(buf, len, "\033[<")) || (len >= 5 && starts_with(buf, len, "\033["))) {
        int mi;
        int mi1 = indexOf(buf, "M");
        int mi2 = indexOf(buf, "m");

        if (mi1 == -1 && mi2 == -1) {
            return 0;
        }

        if (mi1 < mi2 && mi1 != -1) {
            mi = mi1;
        } else {
            mi = mi2;
        }

        bool isM = buf[mi] == 'M';
        bool isU = false;

        if (buf[2] == '<') {
            buf = substring(buf, 3, mi);
        } else {
            isU = true;
            buf = substring(buf, 2, mi);
        }

        int s1 = indexOf(buf, ";");
        int s2 = lastIndexOf(buf, ";");
        // not found or only one ';'
        if (s1 == -1 || s2 == -1 || s1 == s2) {
            return 0;
        }

        int n1 = atoi(substring(buf, 0,s1));
        int n2 = atoi(substring(buf, s1+1, s2));
        int n3 = atoi(substring(buf, s2+1, strlen(buf)));

        if (isU) {
            n1 -= 32;
        }

        switch (n1 & 3) {
            case 0:
                if ((n1 & 64) != 0) {
                    event->key = TB_KEY_MOUSE_WHEEL_UP;
                } else {
                    event->key = TB_KEY_MOUSE_LEFT;
                }
                break;
            case 1:
                if ((n1 & 64) != 0) {
                    event->key = TB_KEY_MOUSE_WHEEL_DOWN;
                } else {
                    event->key = TB_KEY_MOUSE_LEFT;
                }
                break;
            case 2:
                event->key = TB_KEY_MOUSE_RIGHT;
                break;
            case 3:
                event->key = TB_KEY_MOUSE_RELEASE;
                break;
            default:
                return mi + 1;
        }

        if (!isM) {
            event->key = TB_KEY_MOUSE_RELEASE;
        }
        event->type = TB_EVENT_MOUSE;
        if ((n1 & 32) != 0) {
            event->mod |= TB_MOD_MOTION;
        }

        event->x = (uint8_t)n2 - 1;
        event->y = (uint8_t)n3 - 1;
        return mi + 1;
    }



	// it's pretty simple here, find 'starts_with' match and return
	// success, else return failure
	int i;
	for (i = 0; keys[i]; i++) {
		if (starts_with(buf, len, keys[i])) {
			event->ch = 0;
			event->key = 0xFFFF-i;
			return strlen(keys[i]);
		}
	}
	return 0;
}

static bool extract_event(struct tb_event *event, struct bytebuffer *inbuf, int inputmode)
{
	const char *buf = inbuf->buf;
	const int len = inbuf->len;
	if (len == 0)
		return false;

	if (buf[0] == '\033') {
		int n = parse_escape_seq(event, buf, len);
		if (n != 0) {
			bool success = true;
			if (n < 0) {
				success = false;
				n = -n;
			}
			bytebuffer_truncate(inbuf, n);
			return success;
		} else {
			// it's not escape sequence, then it's ALT or ESC,
			// check inputmode
			if (inputmode&TB_INPUT_ESC) {
				// if we're in escape mode, fill ESC event, pop
				// buffer, return success
				event->ch = 0;
				event->key = TB_KEY_ESC;
				event->mod = 0;
				bytebuffer_truncate(inbuf, 1);
				return true;
			} else if (inputmode&TB_INPUT_ALT) {
				// if we're in alt mode, set ALT modifier to
				// event and redo parsing
				event->mod = TB_MOD_ALT;
				bytebuffer_truncate(inbuf, 1);
				return extract_event(event, inbuf, inputmode);
			}
			assert(!"never got here");
		}
	}

	// if we're here, this is not an escape sequence and not an alt sequence
	// so, it's a FUNCTIONAL KEY or a UNICODE character

	// first of all check if it's a functional key
	if ((unsigned char)buf[0] <= TB_KEY_SPACE ||
	    (unsigned char)buf[0] == TB_KEY_BACKSPACE2)
	{
		// fill event, pop buffer, return success */
		event->ch = 0;
		event->key = (uint16_t)buf[0];
		bytebuffer_truncate(inbuf, 1);
		return true;
	}

	// feh... we got utf8 here

	// check if there is all bytes
	if (len >= tb_utf8_char_length(buf[0])) {
		/* everything ok, fill event, pop buffer, return success */
		tb_utf8_char_to_unicode(&event->ch, buf);
		event->key = 0;
		bytebuffer_truncate(inbuf, tb_utf8_char_length(buf[0]));
		return true;
	}

	// event isn't recognized, perhaps there is not enough bytes in utf8
	// sequence
	return false;
}
