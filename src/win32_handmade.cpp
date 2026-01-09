#include "win32_handmade.hpp"

static Screen global_screen = Screen(); // глобальный из-за WindowProc

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	static_assert(DEV_MODE || !SLOW_MODE);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	WNDCLASSA window_class = {
		.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = WindowProc,
		.hInstance = hInstance,
		.lpszClassName = "Handmade_Hero_Window_Class",
	};
	RegisterClassA(&window_class);

	HWND window = CreateWindowExA(0, window_class.lpszClassName, "Handmade Hero", WS_TILEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, nullptr, nullptr, hInstance, nullptr);
	HDC device_context = GetDC(window);

	Game_Code game_code = Game_Code();
	Input input = Input();
	Sound sound = Sound(window);

	void* game_memory_base_address = nullptr;
	if constexpr (DEV_MODE && UINTPTR_MAX == UINT64_MAX) game_memory_base_address = (void*)1024_GB;
	Game::Memory game_memory = {
		.permanent_storage_size = 64_MB,
		.transient_storage_size = 1_GB,
		.permanent_storage = (u8*)VirtualAlloc(game_memory_base_address,
			game_memory.permanent_storage_size + game_memory.transient_storage_size,
			MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE),
		.transient_storage = game_memory.permanent_storage + game_memory.permanent_storage_size,
    	.read_file_sync = Platform::read_file_sync,
    	.write_file_sync = Platform::write_file_sync,
    	.free_file_memory = Platform::free_file_memory,
	};

	bool dev_is_pause = false;
	u64 flip_wall_clock = get_wall_clock();
	u64 flip_cycle_counter = __rdtsc();

	while (true) {
		input.prepare_for_new_frame();
		input.process_gamepad();

		MSG message;
		while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
			switch (message.message) {
				case WM_KEYUP:
				case WM_KEYDOWN: {
					bool is_key_pressed  = !(message.lParam & (1u << 31));
					bool was_key_pressed =   message.lParam & (1u << 30);
					if (is_key_pressed == was_key_pressed) continue;

					input.process_keyboard_button(message.wParam, is_key_pressed);
					if constexpr (DEV_MODE) if (message.wParam == 'P' && is_key_pressed) {
						dev_is_pause = !dev_is_pause;
					}
				} break;
				case WM_QUIT: {
					return (int)message.wParam;
				} break;
				default: {
					TranslateMessage(&message);
					DispatchMessageA(&message);
				}
			}
		}

		if constexpr (DEV_MODE) {
			game_code.dev_reload_if_recompiled();
			if (dev_is_pause) {
				wait_until_end_of_frame(flip_wall_clock);
				flip_wall_clock = get_wall_clock();
				flip_cycle_counter = __rdtsc();
				continue;
			};
		}

		game_code.update_and_render(input.game_input, game_memory, global_screen.game_buffer);
		sound.calc_samples_to_write(flip_wall_clock);
		game_code.get_sound_samples(game_memory, sound.game_buffer);
		sound.submit();
		wait_until_end_of_frame(flip_wall_clock);

		flip_wall_clock = get_wall_clock();
		flip_cycle_counter = __rdtsc();

		if constexpr (DEV_MODE) sound.dev_sync_display(global_screen);
		global_screen.display(window, device_context);
	}

	return EXIT_SUCCESS;
}

static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);
			global_screen.display(window, device_context);
			EndPaint(window, &paint);
		} break;
		case WM_DESTROY: {
			PostQuitMessage(0);
		} break;
		default:
			return DefWindowProcA(window, message, wParam, lParam);
	}

	return 0;
}

static void wait_until_end_of_frame(u64 flip_wall_clock) {
	f32 frame_seconds_elapsed = get_seconds_elapsed(flip_wall_clock);
	if (SLEEP_GRANULARITY_MS) {
		f32 sleep_ms = 1000.0f * (TARGET_SECONDS_PER_FRAME - frame_seconds_elapsed) - (f32)SLEEP_GRANULARITY_MS;
		if (sleep_ms >= 1) Sleep((DWORD)sleep_ms);
	}
	frame_seconds_elapsed = get_seconds_elapsed(flip_wall_clock);
	while (frame_seconds_elapsed < TARGET_SECONDS_PER_FRAME) {
		YieldProcessor();
		frame_seconds_elapsed = get_seconds_elapsed(flip_wall_clock);
	}
}

static FILETIME get_file_write_time(const char* filename) {
	WIN32_FILE_ATTRIBUTE_DATA file_data;
	if (!GetFileAttributesExA(filename, GetFileExInfoStandard, &file_data)) return {};

	return file_data.ftLastWriteTime;
}

static inline f32 get_seconds_elapsed(u64 start) {
	return (f32)(get_wall_clock() - start) / (f32)PERFORMANCE_FREQUENCY;
}

static inline u64 get_wall_clock() {
	LARGE_INTEGER performance_counter_result;
	QueryPerformanceCounter(&performance_counter_result);
	return (u64)performance_counter_result.QuadPart;
}

Game_Code::Game_Code() {
	auto& game_code = *this;
	char main_file_path[MAX_PATH]; // бывают более длинные пути!
	GetModuleFileNameA(nullptr, main_file_path, sizeof(main_file_path));
	char* one_past_last_slash = main_file_path;
	for (char* scan = main_file_path; *scan; scan++) {
		if (*scan == '\\') one_past_last_slash = scan + 1;
	}
	uptr main_folder_path_size = (uptr)(one_past_last_slash - main_file_path);
	
	const char dll_name[] = "handmade.dll";
	utils::str_concat(
		main_file_path, main_folder_path_size,
		dll_name, sizeof(dll_name),
		game_code.dll_path, sizeof(game_code.dll_path)
	);

	const char temp_dll_name[] = "handmade_temp.dll";
	utils::str_concat(
		main_file_path, main_folder_path_size,
		temp_dll_name, sizeof(temp_dll_name),
		game_code.temp_dll_path, sizeof(game_code.temp_dll_path)
	);

	game_code.load();
}

void Game_Code::load() {
	auto& game_code = *this;
	BOOL copy_result = CopyFileA(game_code.dll_path, game_code.temp_dll_path, FALSE);

	if constexpr (DEV_MODE) {
		while (!copy_result && GetLastError() == ERROR_SHARING_VIOLATION) {
			Sleep(1); // ждем в цикле когда Visual Studio освободит новый dll
			copy_result = CopyFileA(game_code.dll_path, game_code.temp_dll_path, FALSE);
		}
	}

	HMODULE loaded_dll = LoadLibraryA(game_code.temp_dll_path); // загружаем копию, чтобы компилятор мог писать в оригинальный файл
	if (!loaded_dll) {
		game_code.update_and_render = Game::update_and_render_stub;
		game_code.get_sound_samples = Game::get_sound_samples_stub;
	} else {
		game_code.dll = loaded_dll;
		game_code.write_time = get_file_write_time(game_code.dll_path);
		game_code.update_and_render = (Game::Update_And_Render*)GetProcAddress(loaded_dll, "update_and_render");
		game_code.get_sound_samples = (Game::Get_Sound_Samples*)GetProcAddress(loaded_dll, "get_sound_samples");
	}
}

void Game_Code::dev_reload_if_recompiled() {
	auto& game_code = *this;
	FILETIME dll_write_time = get_file_write_time(game_code.dll_path);
	bool is_dll_recompiled = CompareFileTime(&dll_write_time, &game_code.write_time) > 0;
	if (is_dll_recompiled) {
		FreeLibrary(game_code.dll);
		game_code.dll = nullptr;
		game_code.load();
	}
}

Input::Input() {
	auto& input = *this;
	HMODULE xinput_dll = LoadLibraryA("xinput1_3.dll");
	if (!xinput_dll) return;
	
	input.XInputGetState = (Xinput_Get_State*)GetProcAddress(xinput_dll, "XInputGetState");
	input.XInputSetState = (Xinput_Set_State*)GetProcAddress(xinput_dll, "XInputSetState");
}

void Input::prepare_for_new_frame() {
	auto& input = *this;

	for (auto& controller : input.game_input.controllers) {
		controller.start.transitions_count 			= 0;
		controller.back.transitions_count  			= 0;
		controller.left_shoulder.transitions_count  = 0;
		controller.right_shoulder.transitions_count = 0;

		controller.move_up.transitions_count    	= 0;
		controller.move_down.transitions_count  	= 0;
		controller.move_left.transitions_count  	= 0;
		controller.move_right.transitions_count 	= 0;

		controller.action_up.transitions_count		= 0;
		controller.action_down.transitions_count	= 0;
		controller.action_left.transitions_count	= 0;
		controller.action_right.transitions_count	= 0;
	}
}

void Input::process_gamepad() {
	auto& input = *this;
	XINPUT_STATE state;
	if (input.XInputGetState(0, &state)) return;
	
	Game::Controller& controller = input.game_input.controllers[1];
	controller.is_analog = true;
	controller.start_x = controller.end_x;
	controller.start_y = controller.end_y;
	controller.end_x = controller.min_x = controller.max_x = get_normalized_stick_value(state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
	controller.end_y = controller.min_y = controller.max_y = get_normalized_stick_value(state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

	process_gamepad_button(controller.start,			state.Gamepad.wButtons & XINPUT_GAMEPAD_START);
	process_gamepad_button(controller.back,				state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);
	process_gamepad_button(controller.left_shoulder, 	state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
	process_gamepad_button(controller.right_shoulder,	state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

	process_gamepad_button(controller.move_up, 			state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
	process_gamepad_button(controller.move_down, 		state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
	process_gamepad_button(controller.move_left, 		state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
	process_gamepad_button(controller.move_right, 		state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

	process_gamepad_button(controller.action_up, 		state.Gamepad.wButtons & XINPUT_GAMEPAD_A);
	process_gamepad_button(controller.action_down, 		state.Gamepad.wButtons & XINPUT_GAMEPAD_B);
	process_gamepad_button(controller.action_left, 		state.Gamepad.wButtons & XINPUT_GAMEPAD_X);
	process_gamepad_button(controller.action_right, 	state.Gamepad.wButtons & XINPUT_GAMEPAD_Y);
}

f32 Input::get_normalized_stick_value(SHORT value, SHORT deadzone) {
	return (-deadzone <= value && value < 0) || (0 < value && value <= deadzone) ? 0 : value / 32768.0f;
}

void Input::process_gamepad_button(Game::Button_State& state, bool is_pressed) {
	state.transitions_count += is_pressed != state.is_pressed;
	state.is_pressed = is_pressed;
}

void Input::process_keyboard_button(WPARAM key_code, bool is_pressed) {
	auto& input = *this;
	Game::Controller& controller = input.game_input.controllers[0];
	Game::Button_State* button_state;
	switch (key_code) {
		case VK_RETURN: button_state = &controller.start;			break;
		case VK_ESCAPE: button_state = &controller.back;			break;
		case VK_UP: 	button_state = &controller.move_up;			break;
		case VK_DOWN: 	button_state = &controller.move_down;		break;
		case VK_LEFT: 	button_state = &controller.move_left;		break;
		case VK_RIGHT: 	button_state = &controller.move_right;		break;
		case 'W': 		button_state = &controller.action_up;		break;
		case 'S': 		button_state = &controller.action_down;		break;
		case 'A': 		button_state = &controller.action_left;		break;
		case 'D': 		button_state = &controller.action_right;	break;
		case 'Q': 		button_state = &controller.left_shoulder;	break;
		case 'E': 		button_state = &controller.right_shoulder;	break;
		default: return;
	}
	button_state->is_pressed = is_pressed;
	button_state->transitions_count++;
}

Screen::Screen() {
	Screen::resize(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT);
}

void Screen::resize(u32 width, u32 height) {
	auto& screen = *this;
	if (width == screen.game_buffer.width && height == screen.game_buffer.height) return;

	screen.game_buffer.width  = width;
	screen.game_buffer.height = height;
	screen.bitmap_info.bmiHeader.biWidth  =   (LONG)width;
	screen.bitmap_info.bmiHeader.biHeight = - (LONG)height; // отрицательный чтобы верхний левый пиксель был первым в буфере
	uptr memory_size = width * height * sizeof(*screen.game_buffer.memory);
	VirtualFree(screen.game_buffer.memory, 0, MEM_RELEASE);
	screen.game_buffer.memory = (u32*)VirtualAlloc(nullptr, memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void Screen::display(HWND window, HDC device_context) const {
	auto& screen = *this;
	RECT client_rect;
	GetClientRect(window, &client_rect);

	int dest_width = client_rect.right - client_rect.left;
	int dest_height = client_rect.bottom - client_rect.top;
	int src_width = (int)screen.game_buffer.width;
	int src_height = (int)screen.game_buffer.height;

	StretchDIBits(device_context,
		0, 0, dest_width, dest_height,
		0, 0, src_width, src_height,
		screen.game_buffer.memory, &screen.bitmap_info,
		DIB_RGB_COLORS, SRCCOPY
	);
}

void Screen::dev_draw_vertical(u32 x, u32 top, u32 bottom, u32 color) {
	auto& screen = *this;
	u32* pixel = screen.game_buffer.memory + top * screen.game_buffer.width + x;

	for (u32 y = top; y < bottom; y++) {
		*pixel = color;
		pixel += screen.game_buffer.width;
	}
}

Sound::Sound(HWND window) {
	auto& sound = *this;
	sound.game_buffer = {
		.samples_per_second = sound.wave_format.nSamplesPerSec,
		.samples = (Game::Sound_Sample*)VirtualAlloc(nullptr, sound.get_buffer_size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE),
	};

	HMODULE direct_sound_dll = LoadLibraryA("dsound.dll");
	if (!direct_sound_dll) return;

	typedef HRESULT WINAPI Direct_Sound_Create(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);
	Direct_Sound_Create* DirectSoundCreate = (Direct_Sound_Create*)GetProcAddress(direct_sound_dll, "DirectSoundCreate");
	IDirectSound* direct_sound;
	if (!SUCCEEDED(DirectSoundCreate(nullptr, &direct_sound, nullptr))) return;
	if (!SUCCEEDED(direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY))) return;

	DSBUFFERDESC primary_buffer_desc = {
		.dwSize = sizeof(primary_buffer_desc),
		.dwFlags = DSBCAPS_PRIMARYBUFFER
	};
	IDirectSoundBuffer* primary_buffer;
	if (!SUCCEEDED(direct_sound->CreateSoundBuffer(&primary_buffer_desc, &primary_buffer, nullptr))) return;
	if (!SUCCEEDED(primary_buffer->SetFormat(&sound.wave_format))) return;

	DSBUFFERDESC sound_buffer_desc = {
		.dwSize = sizeof(sound_buffer_desc),
		.dwBufferBytes = sound.get_buffer_size(),
		.lpwfxFormat = &sound.wave_format
	};
	if (!SUCCEEDED(direct_sound->CreateSoundBuffer(&sound_buffer_desc, &sound.buffer, nullptr))) return;

	void *region1, *region2; DWORD region1_size, region2_size;
	if(!SUCCEEDED(sound.buffer->Lock(0, sound.get_buffer_size(), &region1, &region1_size, &region2, &region2_size, 0))) return;
	utils::memset(region1, 0, region1_size);
	utils::memset(region2, 0, region2_size);
	sound.buffer->Unlock(region1, region1_size, region2, region2_size);

	// TODO: address sanitizer падает после вызова Play(), по-видимому это известная проблема DirectSound
	// https://stackoverflow.com/questions/72511236/directsound-crashes-due-to-a-read-access-violation-when-calling-idirectsoundbuff
	sound.buffer->Play(0, 0, DSBPLAY_LOOPING);
}

void Sound::calc_samples_to_write(u64 flip_wall_clock) {
	// Определяем величину, на размер которой может отличаться время цикла (safety_bytes). Когда мы просыпаемся чтобы писать звук,
	// смотрим где находится play_cursor и делаем прогноз где от будет находиться при смене кадра (expected_flip_play_cursor_unwrapped).
	// Если write_cursor + safety_bytes < expected_flip_play_cursor_unwrapped, то это значит что у нас звуковая карта с маленькой задержкой
	// и мы успеваем писать звук синхронно с изображением, поэтому пишем звук до конца следующего фрейма.
	// Если write_cursor + safety_bytes >= expected_flip_play_cursor_unwrapped, то полностью синхронизировать звук и изображение не получится,
	// просто пишем количество сэмплов, равное bytes_per_frame + safety_bytes

	auto& sound = *this;
	DWORD play_cursor = 0, write_cursor = 0;
	if (!sound.buffer || !SUCCEEDED(sound.buffer->GetCurrentPosition(&play_cursor, &write_cursor))) {
		sound.game_buffer.samples_to_write = 0;
		if constexpr (DEV_MODE) sound.dev_markers[sound.dev_markers_index] = {};
		return;
	}

	f32 from_flip_to_audio_seconds = get_seconds_elapsed(flip_wall_clock);
	DWORD expected_bytes_until_flip = sound.get_bytes_per_frame() - (DWORD)((f32)sound.wave_format.nAvgBytesPerSec * from_flip_to_audio_seconds);
	DWORD expected_flip_play_cursor_unwrapped = play_cursor + expected_bytes_until_flip;
	DWORD write_cursor_unwrapped = play_cursor < write_cursor
		? write_cursor
		: write_cursor + sound.get_buffer_size();

	bool is_low_latency_sound = (write_cursor_unwrapped + sound.get_safety_bytes()) < expected_flip_play_cursor_unwrapped;
	DWORD target_cursor_unwrapped = is_low_latency_sound
		? expected_flip_play_cursor_unwrapped + sound.get_bytes_per_frame()
		: write_cursor_unwrapped + sound.get_bytes_per_frame() + sound.get_safety_bytes();

	DWORD target_cursor = target_cursor_unwrapped % sound.get_buffer_size();
	DWORD output_byte_count = sound.get_output_location() < target_cursor ? 0 : sound.get_buffer_size();
	output_byte_count += target_cursor - sound.get_output_location();
	sound.game_buffer.samples_to_write = output_byte_count / sound.wave_format.nBlockAlign;

	if constexpr (DEV_MODE) {
		sound.dev_markers[sound.dev_markers_index] = {
			.output_play_cursor = play_cursor,
			.output_write_cursor = write_cursor,
			.output_location = sound.get_output_location(),
			.output_byte_count = output_byte_count,
			.expected_flip_play_cursor = expected_flip_play_cursor_unwrapped % sound.get_buffer_size()
		};
	}
}

void Sound::submit() {
	auto& sound = *this;
	void *region1, *region2; DWORD region1_size, region2_size;
	if (!sound.buffer || !SUCCEEDED(sound.buffer->Lock(sound.get_output_location(),
		sound.game_buffer.samples_to_write * sound.wave_format.nBlockAlign,
		&region1, &region1_size, &region2, &region2_size, 0))) {
		return;
	}

	DWORD sample_size = sizeof(*(sound.game_buffer.samples));
	DWORD region1_size_samples = region1_size / sample_size;
	DWORD region2_size_samples = region2_size / sample_size;
	sound.running_sample_index += region1_size_samples + region2_size_samples;

	utils::memcpy(region1, sound.game_buffer.samples, region1_size);
	utils::memcpy(region2, sound.game_buffer.samples + region1_size_samples, region2_size);
	sound.buffer->Unlock(region1, region1_size, region2, region2_size);
}

void Sound::dev_sync_display(Screen& screen) {
	auto& sound = *this;
	if (!sound.buffer) return;

	f32 horizontal_scaling = (f32)screen.game_buffer.width / (f32)sound.get_buffer_size();
	for (uptr i = 0; i < utils::array_count(sound.dev_markers); i++) {
		u32 top = 0;
		u32 bottom = screen.game_buffer.height * 1/4;
		u32 historic_output_play_cursor_x = (u32)((f32)sound.dev_markers[i].output_play_cursor * horizontal_scaling);
		u32 historic_output_write_cursor_x = (u32)((f32)sound.dev_markers[i].output_write_cursor * horizontal_scaling);
		screen.dev_draw_vertical(historic_output_play_cursor_x, top, bottom, 0x00ffffff);
		screen.dev_draw_vertical(historic_output_write_cursor_x, top, bottom, 0x00ff0000);
	}

	if (!sound.game_buffer.samples_to_write || !SUCCEEDED(sound.buffer->GetCurrentPosition(&sound.dev_markers[sound.dev_markers_index].flip_play_cursor, nullptr))) {
		sound.dev_markers_index = (sound.dev_markers_index + 1) % utils::array_count(sound.dev_markers);
		return;
	}

	Dev_Marker current_marker = sound.dev_markers[sound.dev_markers_index];
	u32 expected_flip_play_cursor_x = (u32)((f32)current_marker.expected_flip_play_cursor * horizontal_scaling);
	screen.dev_draw_vertical(expected_flip_play_cursor_x, 0, screen.game_buffer.height, 0xffffff00);

	{
		u32 top 	= screen.game_buffer.height * 1/4;
		u32 bottom  = screen.game_buffer.height * 2/4;
		u32 output_play_cursor_x = (u32)((f32)current_marker.output_play_cursor * horizontal_scaling);
		u32 output_write_cursor_x = (u32)((f32)current_marker.output_write_cursor * horizontal_scaling);
		screen.dev_draw_vertical(output_play_cursor_x, top, bottom, 0x00ffffff);
		screen.dev_draw_vertical(output_write_cursor_x, top, bottom, 0x00ff0000);
	}
	{
		u32 top 	= screen.game_buffer.height * 2/4;
		u32 bottom  = screen.game_buffer.height * 3/4;
		u32 output_location_x = (u32)((f32)current_marker.output_location * horizontal_scaling);
		u32 output_byte_count_x = (u32)((f32)current_marker.output_byte_count * horizontal_scaling);
		screen.dev_draw_vertical(output_location_x, top, bottom, 0x00ffffff);
		screen.dev_draw_vertical((output_location_x + output_byte_count_x) % screen.game_buffer.width, top, bottom, 0x00ff0000);
	}
	{
		u32 top 	= screen.game_buffer.height * 3/4;
		u32 bottom  = screen.game_buffer.height;
		u32 flip_play_cursor_x = (u32)((f32)current_marker.flip_play_cursor * horizontal_scaling);
		u32 safety_bytes_x = (u32)((f32)sound.get_safety_bytes() * horizontal_scaling);
		screen.dev_draw_vertical((flip_play_cursor_x - safety_bytes_x / 2) % screen.game_buffer.width, top, bottom, 0x00ffffff);
		screen.dev_draw_vertical((flip_play_cursor_x + safety_bytes_x / 2) % screen.game_buffer.width, top, bottom, 0x00ffffff);
	}

	// uptr dev_markers_index_unwrapped = sound.dev_markers_index == 0 ? utils::array_count(sound.dev_markers) : sound.dev_markers_index;
	// Debug_Marker prev_marker = sound.dev_markers[dev_markers_index_unwrapped - 1];
	// DWORD prev_sound_filled_cursor = (prev_marker.output_location + prev_marker.output_byte_count) % sound.get_buffer_size();
	// // сравниваем с половиной размера буфера чтобы учесть что prev_sound_filled_cursor мог сделать оборот
	// assert(sound.play_cursor <= prev_sound_filled_cursor || sound.play_cursor > prev_sound_filled_cursor + sound.get_buffer_size() / 2);
	
	// f32 ms_per_frame = 1000 * frame_seconds_elapsed;
	// u64 cycle_counter_elapsed = __rdtsc() - flip_cycle_counter;
	// char output_buffer[256];
	// sprintf_s(output_buffer, "frame ms: %.2f\n", frame_seconds_elapsed * 1000);
	// OutputDebugStringA(output_buffer);

	sound.dev_markers_index = (sound.dev_markers_index + 1) % utils::array_count(sound.dev_markers);
}

namespace Platform {
	Read_File_Result read_file_sync(const char* file_name) {
		Read_File_Result result = {};
		u32 memory_size = 0;
		void* memory = nullptr;

		HANDLE heap_handle = GetProcessHeap();
		if (!heap_handle) return result;

		HANDLE file_handle = CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if (file_handle == INVALID_HANDLE_VALUE) return result;

		LARGE_INTEGER file_size;
		if (!GetFileSizeEx(file_handle, &file_size)) goto close_file_handle;

		assert(file_size.QuadPart <= UINT32_MAX);
		memory_size = (u32)file_size.QuadPart;
		memory = HeapAlloc(heap_handle, 0, memory_size);
		if (!memory) goto close_file_handle;

		DWORD bytes_read;
		if (!ReadFile(file_handle, memory, memory_size, &bytes_read, nullptr) || (bytes_read != memory_size)) {
			HeapFree(heap_handle, 0, memory);
			goto close_file_handle;
		}

		result.memory_size = memory_size;
		result.memory = memory;

		close_file_handle:
			CloseHandle(file_handle);
			return result;
	}

	bool write_file_sync(const char* file_name, const void* memory, u32 memory_size) {
		bool result = false;
		HANDLE file_handle = CreateFileA(file_name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
		if (file_handle == INVALID_HANDLE_VALUE) return result;

		DWORD bytes_written;
		if (WriteFile(file_handle, memory, memory_size, &bytes_written, nullptr) && (bytes_written == memory_size)) {
			result = true;
		}
		CloseHandle(file_handle);
		return result;
	}
	
	void free_file_memory(void* memory) {
		if (!memory) return;

		HANDLE heap_handle = GetProcessHeap();
		if (!heap_handle) return;

		HeapFree(heap_handle, 0, memory);
		memory = nullptr;
	}
}