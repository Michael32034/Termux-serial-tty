import serial
import time


class S(serial.Serial):
    def _update_dtr_state(self):
        pass

    def _update_rts_state(self):
        pass

    def _update_break_state(self):
        pass


s = S("./sysi", 9600, timeout=1)
s.write(b"x")
time.sleep(0.1)
print(s.read(100))
s.close()
