import os, pathlib, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[4]
tests=pathlib.Path(__file__).resolve().parent
out=pathlib.Path(tempfile.gettempdir())/'codex-artifacts/shared-iic0-tests'
out.mkdir(parents=True,exist_ok=True)
for name in ['r_i2c_master_api.h','bsp_api.h','r_iic_master.h','r_sci_b_i2c.h','FreeRTOS.h','semphr.h','hal_data.h']:
 (out/name).write_text('#include "stubs.h"\n')
camera=root/'ports/renesas-ra8/boards/CPKCOR_RA8P1/e2studio_gcc_freertos/src/camera'
cc=os.environ.get('CC','gcc')
cmd=[cc,'-std=c99','-Wall','-Wextra','-Werror','-DBSP_CFG_RTOS=2','-I'+str(camera),'-I'+str(out),'-I'+str(tests),'-I'+str(root/'ports/renesas-ra8/peripheral'),str(root/'ports/renesas-ra8/peripheral/i2c.c'),str(tests/'test_shared_iic0.c'),str(tests/'test_camera_board.c'),str(camera/'camera_board.c'),'-o',str(out/'test.exe')]
subprocess.run(cmd,check=True)
subprocess.run([str(out/'test.exe')],check=True)
