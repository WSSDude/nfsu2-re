static
__declspec(naked)
void *__stdcall find_nearest_marker(void *_near, int type, void *after)
{
	_asm {
		mov ecx, 0x88F298
		mov eax, 0x5D9BF0
		jmp eax
	}
}

static
void debug_custom_find_nearest_marker(int wparam)
{
	void *nearest;
	int *a;
	int *b;

	if (wparam == 121) { // y
		a = *(int**) 0x8900B4;
		if (a) {
			b = (void*) *(a + 1);
			b += 0x60 / 4;
			nearest = find_nearest_marker(b, 0x11, 0);
			log(buf, sprintf(buf, "my pos %.1f %.1f %.1f nearest %p",
				*(float*) b, *(float*) (b + 1), *(float*) (b + 2),
				nearest));

			/*push text like the area text in (career) freeroam*/
			b = (void*) *(a + 0x9A4 / 4);
			b = (void*) *(b + 0x74 / 4);
			if (b) {
				b = (void*) *(b + 0x20 / 4);
				((void(__cdecl *)(void*,char*,char*))0x537BE0)(b,"OKEY","HEY THERE");
				((void(__cdecl *)(void*,char*,int))0x51CF70)(b,"ZoomIn",1);
			}
		} else {
			log(buf, sprintf(buf, "can't get position"));
		}
	}

	DEBUG_WMCHAR_FUNC(wparam);
}
#undef DEBUG_WMCHAR_FUNC
#define DEBUG_WMCHAR_FUNC debug_custom_find_nearest_marker
