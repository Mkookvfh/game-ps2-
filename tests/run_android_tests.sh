#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp_dir=$(mktemp -d)
trap 'rm -f "$tmp_dir/test" "$tmp_dir/progress" "$tmp_dir/rigged.glb" "$tmp_dir/prototype.glb"; rmdir "$tmp_dir"' EXIT
cd "$repo_dir"

for test in android_game_health_test android_stealth_test android_rival_test android_exploration_test; do
    ${CC:-cc} -std=c11 -Wall -Wextra -Werror -Ishared \
        "tests/$test.c" shared/game.c -lm -o "$tmp_dir/test"
    "$tmp_dir/test"
done

${CC:-cc} -std=c11 -Wall -Wextra -Werror -Iplatform/android/app/src/main/cpp \
    tests/android_resource_test.c platform/android/app/src/main/cpp/resource_system.c \
    -lm -o "$tmp_dir/test"
"$tmp_dir/test"

${CC:-cc} -std=c11 -Wall -Wextra -Werror -Ishared -Iplatform/android/app/src/main/cpp \
    tests/android_progress_test.c platform/android/app/src/main/cpp/progress.c \
    platform/android/app/src/main/cpp/resource_system.c shared/game.c \
    -lm -o "$tmp_dir/test"
"$tmp_dir/test" "$tmp_dir/progress"

python3 tests/make_rigged_fixture.py "$tmp_dir/rigged.glb"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -Iplatform/android/app/src/main/cpp \
    tests/android_rigged_model_test.c platform/android/app/src/main/cpp/rigged_model.c \
    -lm -o "$tmp_dir/test"
"$tmp_dir/test" "$tmp_dir/rigged.glb"

python3 platform/android/tools/generate_rigged_character.py \
    platform/android/app/src/main/assets/models/MUEANG_ISARA_Main_Assassin_02.glb "$tmp_dir/prototype.glb"
cmp "$tmp_dir/prototype.glb" platform/android/app/src/main/assets/models/MUEANG_ISARA_Rigged_Prototype.glb
${CC:-cc} -std=c11 -Wall -Wextra -Werror -Iplatform/android/app/src/main/cpp \
    tests/android_generated_rig_test.c platform/android/app/src/main/cpp/rigged_model.c \
    -lm -o "$tmp_dir/test"
"$tmp_dir/test" "$tmp_dir/prototype.glb" \
    platform/android/app/src/main/assets/models/MUEANG_ISARA_Main_Assassin_02.glb
