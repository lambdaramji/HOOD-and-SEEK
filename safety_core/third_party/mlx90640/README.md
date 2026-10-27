# MLX90640 API (vendored)

- 출처: <https://github.com/pimoroni/mlx90640-library> 커밋 `a92229a` (Melexis `mlx90640-library`의 포크)
- 라이선스: Apache-2.0 — 같은 폴더의 `LICENSE` 원문. 각 파일 머리의 저작권 표기를 지우지 않습니다.
- 가져온 파일: `MLX90640_API.cpp`·`MLX90640_API.h`·`MLX90640_I2C_Driver.h` (수정 없음)
- 가져오지 않은 것: 포크의 I2C 드라이버(`MLX90640_LINUX_I2C_Driver.cpp` 등). 열기 실패 뒤 다시 열지 않고 `/dev/i2c-1`을 SPS30과 따로 열어서, SafetyCore는 `src/sensors.cpp`가 `MLX90640_I2C_Driver.h`의 네 함수를 직접 구현합니다.
- 10/19 #45 실측에 쓴 `mlx_test`·`mlx_stream`(inbox/정정인/mlx_bundle.sh)과 같은 커밋입니다.
