import subprocess
import winsound
import time

while True:
    try:
        proc = subprocess.run(
            ["JLink.exe", "-Device", "CORTEX-M33", "-NoGui", "1",  "-CommandFile", "config.jlink"],
            # ["ezFlashCLI", "probe"],
            input=b"r\nexit\n",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=10
        )

        # Decode output safely
        output = proc.stdout.decode("windows-1252") + proc.stderr.decode("windows-1252")
        print(output)

        if "Device not responding" in output or "Cannot connect to target" in output:
            print("Target Not found!")
            winsound.Beep(500, 100)
        elif "Cannot connect to J-Link" in output:
            print("J-Link Not found!")
            winsound.Beep(500, 100)
        else:
            print("Target Found!")
            for _ in range(4):
                winsound.Beep(4000, 100)
                winsound.Beep(1000, 100)
        
        time.sleep(1)

    except subprocess.TimeoutExpired:
        print("Command timed out.")
        winsound.Beep(300, 200)

    except Exception as e:
        print(f"Unexpected error: {e}")
        winsound.Beep(200, 300)
