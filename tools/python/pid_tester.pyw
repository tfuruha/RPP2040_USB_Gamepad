import tkinter as tk
import customtkinter as ctk
import hid
import serial
import serial.tools.list_ports
import threading
import struct
import time

# UIのテーマ設定
ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

class PIDTesterApp(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("PID Protocol Parser Tester")
        self.geometry("1000x600")

        # 通信インスタンス
        self.hid_device = None
        self.serial_inst = None
        self.running = True

        self.setup_ui()
        
        # ログ更新用スレッド開始
        self.log_thread = threading.Thread(target=self.serial_read_task, daemon=True)
        self.log_thread.start()

    def setup_ui(self):
        # グリッド構成
        self.grid_columnconfigure(0, weight=1)  # ログエリア
        self.grid_columnconfigure(1, weight=0)  # 操作パネル
        self.grid_rowconfigure(0, weight=1)

        # --- 左側: ログエリア ---
        self.log_frame = ctk.CTkFrame(self)
        self.log_frame.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")
        self.log_frame.grid_rowconfigure(1, weight=1)
        self.log_frame.grid_columnconfigure(0, weight=1)

        self.log_label = ctk.CTkLabel(self.log_frame, text="Device Serial Log", font=ctk.CTkFont(size=14, weight="bold"))
        self.log_label.grid(row=0, column=0, padx=10, pady=(10, 5), sticky="w")

        self.log_box = ctk.CTkTextbox(self.log_frame, state="disabled")
        self.log_box.grid(row=1, column=0, padx=10, pady=5, sticky="nsew")

        self.clear_btn = ctk.CTkButton(self.log_frame, text="Clear Log", command=self.clear_log)
        self.clear_btn.grid(row=2, column=0, padx=10, pady=10)

        # --- 右側: 操作パネル ---
        self.ctrl_frame = ctk.CTkScrollableFrame(self, width=400)
        self.ctrl_frame.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")

        # 接続設定
        self.setup_conn_ui()

        # PID Test: Set Effect (0x01)
        self.setup_effect_ui()

        # PID Test: Set Constant Force (0x05)
        self.setup_constant_force_ui()

        # PID Test: Device Gain (0x0D)
        self.setup_device_gain_ui()

        # PID Test: Effect Operation (0x0A)
        self.setup_effect_operation_ui()

    def setup_conn_ui(self):
        frame = self.create_section_frame("Connection")
        
        # Serial Port
        self.port_label = ctk.CTkLabel(frame, text="Serial Port:")
        self.port_label.pack(anchor="w", padx=5)
        self.port_combo = ctk.CTkComboBox(frame, values=self.get_serial_ports())
        self.port_combo.pack(fill="x", padx=5, pady=2)
        
        # HID Device
        self.hid_label = ctk.CTkLabel(frame, text="HID Device (Filter: Gamepad):")
        self.hid_label.pack(anchor="w", padx=5, pady=(5, 0))
        self.hid_combo = ctk.CTkComboBox(frame, values=self.get_hid_devices())
        self.hid_combo.pack(fill="x", padx=5, pady=2)

        self.conn_btn = ctk.CTkButton(frame, text="Connect", command=self.toggle_connection)
        self.conn_btn.pack(fill="x", padx=5, pady=10)

    def setup_effect_ui(self):
        frame = self.create_section_frame("Set Effect (ID: 0x01)")
        
        ctk.CTkLabel(frame, text="Effect Type (0x26=Constant):").pack(anchor="w", padx=5)
        self.effect_type = ctk.CTkEntry(frame)
        self.effect_type.insert(0, "0x26")
        self.effect_type.pack(fill="x", padx=5, pady=2)

        ctk.CTkLabel(frame, text="Gain (0-32767):").pack(anchor="w", padx=5, pady=(5,0))
        
        gain_frame = ctk.CTkFrame(frame, fg_color="transparent")
        gain_frame.pack(fill="x", padx=5, pady=2)
        
        self.effect_gain = ctk.CTkSlider(gain_frame, from_=0, to=32767, number_of_steps=32767, 
                                          command=lambda v: self.effect_gain_val.configure(text=str(int(v))))
        self.effect_gain.set(16383)
        self.effect_gain.pack(side="left", fill="x", expand=True)
        
        self.effect_gain_val = ctk.CTkLabel(gain_frame, text="16383", width=60)
        self.effect_gain_val.grid_forget() # pack inside frame
        self.effect_gain_val.pack(side="right", padx=5)

        self.send_effect_btn = ctk.CTkButton(frame, text="Send Report 0x01", command=self.send_report_01)
        self.send_effect_btn.pack(fill="x", padx=5, pady=10)

    def setup_constant_force_ui(self):
        frame = self.create_section_frame("Set Constant Force (ID: 0x05)")
        
        ctk.CTkLabel(frame, text="Magnitude (-32767 to 32767):").pack(anchor="w", padx=5)
        
        mag_frame = ctk.CTkFrame(frame, fg_color="transparent")
        mag_frame.pack(fill="x", padx=5, pady=2)
        
        self.const_mag = ctk.CTkSlider(mag_frame, from_=-32767, to=32767, number_of_steps=65534,
                                       command=lambda v: self.const_mag_val.configure(text=str(int(v))))
        self.const_mag.set(0)
        self.const_mag.pack(side="left", fill="x", expand=True)
        
        self.const_mag_val = ctk.CTkLabel(mag_frame, text="0", width=60)
        self.const_mag_val.pack(side="right", padx=5)

        self.send_const_btn = ctk.CTkButton(frame, text="Send Report 0x05", command=self.send_report_05)
        self.send_const_btn.pack(fill="x", padx=5, pady=10)

    def setup_device_gain_ui(self):
        frame = self.create_section_frame("Device Gain (ID: 0x0D)")
        
        ctk.CTkLabel(frame, text="Device Gain (0-255):").pack(anchor="w", padx=5)
        
        dev_frame = ctk.CTkFrame(frame, fg_color="transparent")
        dev_frame.pack(fill="x", padx=5, pady=2)
        
        self.dev_gain = ctk.CTkSlider(dev_frame, from_=0, to=255, number_of_steps=255,
                                      command=lambda v: self.dev_gain_val.configure(text=str(int(v))))
        self.dev_gain.set(255)
        self.dev_gain.pack(side="left", fill="x", expand=True)
        
        self.dev_gain_val = ctk.CTkLabel(dev_frame, text="255", width=60)
        self.dev_gain_val.pack(side="right", padx=5)

        self.send_gain_btn = ctk.CTkButton(frame, text="Send Report 0x0D", command=self.send_report_0D)
        self.send_gain_btn.pack(fill="x", padx=5, pady=10)

    def setup_effect_operation_ui(self):
        frame = self.create_section_frame("Effect Operation (ID: 0x0A)")
        
        ctk.CTkLabel(frame, text="Operation (1:Start, 2:Solo, 3:Stop):").pack(anchor="w", padx=5)
        
        op_frame = ctk.CTkFrame(frame, fg_color="transparent")
        op_frame.pack(fill="x", padx=5, pady=2)
        
        self.eff_op = ctk.CTkSlider(op_frame, from_=1, to=3, number_of_steps=2,
                                     command=lambda v: self.eff_op_val.configure(text=self.get_op_text(int(v))))
        self.eff_op.set(1)
        self.eff_op.pack(side="left", fill="x", expand=True)
        
        self.eff_op_val = ctk.CTkLabel(op_frame, text="Start", width=60)
        self.eff_op_val.pack(side="right", padx=5)

        self.send_op_btn = ctk.CTkButton(frame, text="Send Report 0x0A", command=self.send_report_0A)
        self.send_op_btn.pack(fill="x", padx=5, pady=10)

    def get_op_text(self, op):
        if op == 1: return "Start"
        if op == 2: return "Solo"
        if op == 3: return "Stop"
        return str(op)

    def create_section_frame(self, title):
        frame = ctk.CTkFrame(self.ctrl_frame)
        frame.pack(fill="x", padx=5, pady=5)
        label = ctk.CTkLabel(frame, text=title, font=ctk.CTkFont(size=12, weight="bold"))
        label.pack(anchor="w", padx=5, pady=5)
        return frame

    # --- 通信ロジック ---
    def get_serial_ports(self):
        return [p.device for p in serial.tools.list_ports.comports()]

    def get_hid_devices(self):
        devs = []
        for d in hid.enumerate():
            usage_page = d.get('usage_page', 0)
            usage = d.get('usage', 0)
            
            # フィルタリング条件: 
            # 1. usage_page == 0x01 (Generic Desktop) かつ usage == 0x05 (Gamepad)
            # 2. または、usage_page == 0x01 (Generic Desktop) かつ usage == 0x04 (Joystick)
            is_gamepad = (usage_page == 0x01 and usage in [0x04, 0x05])
            
            if is_gamepad:
                name = d.get('product_string')
                if not name:
                    name = f"Unknown ({hex(d['vendor_id'])}:{hex(d['product_id'])})"
                
                path = d.get('path', b'').decode(errors='ignore')
                devs.append(f"{name} ({path})")
                
        # フィルタで見つからない場合は全てのデバイスを表示（予備）
        if not devs:
            for d in hid.enumerate():
                name = d.get('product_string') or "Unknown"
                path = d.get('path', b'').decode(errors='ignore')
                devs.append(f"{name} ({path})")
                
        return devs if devs else ["No HID Devices found"]

    def toggle_connection(self):
        if self.hid_device or self.serial_inst:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        try:
            # Serial Connect
            port = self.port_combo.get()
            if port:
                self.serial_inst = serial.Serial(port, 115200, timeout=0.1)
                self.add_log(f"Connected to Serial: {port}")

            # HID Connect
            hid_str = self.hid_combo.get()
            if "(" in hid_str:
                path = hid_str.split("(")[-1].strip(")")
                self.hid_device = hid.device()
                self.hid_device.open_path(path.encode())
                self.add_log(f"Connected to HID: {hid_str}")
            
            self.conn_btn.configure(text="Disconnect", fg_color="red")
        except Exception as e:
            self.add_log(f"Connection Error: {str(e)}")
            self.disconnect()

    def disconnect(self):
        if self.serial_inst:
            self.serial_inst.close()
            self.serial_inst = None
        if self.hid_device:
            self.hid_device.close()
            self.hid_device = None
        self.conn_btn.configure(text="Connect", fg_color=["#3B8ED0", "#1F6AA5"])
        self.add_log("Disconnected.")

    def serial_read_task(self):
        while self.running:
            if self.serial_inst and self.serial_inst.is_open:
                try:
                    line = self.serial_inst.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self.add_log(f"  [DEVICE] {line}")
                except:
                    pass
            time.sleep(0.01)

    def send_report_01(self):
        if not self.hid_device:
            self.add_log("Error: HID Device not connected")
            return
        
        try:
            e_type = int(self.effect_type.get(), 16)
            gain = int(self.effect_gain.get())
            
            # Report ID 0x01: Set Effect (15 bytes total incl. ID)
            # struct format: < B B B H H h B B H H
            # (reportId, blockIdx, type, duration, triggerInterval, gain, triggerBtn, enableAxis, direction, startDelay)
            data = struct.pack("<BBBHHhBBHH", 
                0x01, # reportId
                1,    # blockIndex
                e_type, 
                0, 0, # duration, triggerInterval
                gain, 
                0, 0x04, # triggerButton, enableAxis (Direction Enable)
                0x0000, 0x0000    # direction, startDelay
            )
            self.hid_device.write(data)
            self.add_log(f"Sent Report 0x01: Type={hex(e_type)}, Gain={gain}")
        except Exception as e:
            self.add_log(f"Send Error (01): {str(e)}")

    def send_report_05(self):
        if not self.hid_device:
            self.add_log("Error: HID Device not connected")
            return
        
        try:
            mag = int(self.const_mag.get())
            # Report ID 0x05: Set Constant Force (4 bytes total incl. ID)
            # (reportId, blockIdx, magnitude)
            data = struct.pack("<BBh", 0x05, 1, mag)
            self.hid_device.write(data)
            self.add_log(f"Sent Report 0x05: Mag={mag}")
        except Exception as e:
            self.add_log(f"Send Error (05): {str(e)}")

    def send_report_0D(self):
        if not self.hid_device:
            self.add_log("Error: HID Device not connected")
            return
        
        try:
            gain = int(self.dev_gain.get())
            # Report ID 0x0D: Device Gain (3 bytes total incl. ID)
            # (reportId, gain)
            data = struct.pack("<BB", 0x0D, gain)
            self.hid_device.write(data)
            self.add_log(f"Sent Report 0x0D: Gain={gain}")
        except Exception as e:
            self.add_log(f"Send Error (0D): {str(e)}")

    def send_report_0A(self):
        if not self.hid_device:
            self.add_log("Error: HID Device not connected")
            return
        
        try:
            op = int(self.eff_op.get())
            # Report ID 0x0A: Effect Operation (4 bytes total incl. ID)
            # data: [ID, BlockIdx, Operation, LoopCount]
            data = struct.pack("<BBBB", 0x0A, 1, op, 0xFF)
            self.hid_device.write(data)
            self.add_log(f"Sent Report 0x0A: Op={self.get_op_text(op)}, Loop=255")
        except Exception as e:
            self.add_log(f"Send Error (0A): {str(e)}")

    def add_log(self, text):
        self.log_box.configure(state="normal")
        self.log_box.insert("end", text + "\n")
        self.log_box.see("end")
        self.log_box.configure(state="disabled")

    def clear_log(self):
        self.log_box.configure(state="normal")
        self.log_box.delete("1.0", "end")
        self.log_box.configure(state="disabled")

    def on_closing(self):
        self.running = False
        self.disconnect()
        self.destroy()

if __name__ == "__main__":
    app = PIDTesterApp()
    app.protocol("WM_DELETE_WINDOW", app.on_closing)
    app.mainloop()
