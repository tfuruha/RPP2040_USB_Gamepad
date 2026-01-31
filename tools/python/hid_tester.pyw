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

class HIDTesterApp(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("HID Input Loopback Tester")
        self.geometry("1100x700")

        # 通信インスタンス
        self.hid_device = None
        self.serial_inst = None
        self.running = True

        self.setup_ui()
        
        # 受信スレッド開始
        self.serial_thread = threading.Thread(target=self.serial_read_task, daemon=True)
        self.serial_thread.start()
        
        self.hid_thread = threading.Thread(target=self.hid_read_task, daemon=True)
        self.hid_thread.start()

    def setup_ui(self):
        self.grid_columnconfigure(0, weight=1)  # メインパネル
        self.grid_rowconfigure(0, weight=1)

        # トップレベルの分割
        self.main_container = ctk.CTkFrame(self)
        self.main_container.pack(fill="both", expand=True, padx=10, pady=10)
        
        self.main_container.grid_columnconfigure(0, weight=1) # Left: Control
        self.main_container.grid_columnconfigure(1, weight=1) # Right: Monitor
        self.main_container.grid_rowconfigure(0, weight=1)

        # --- 左側: 送信コントロール ---
        self.left_panel = ctk.CTkScrollableFrame(self.main_container)
        self.left_panel.grid(row=0, column=0, padx=5, pady=5, sticky="nsew")
        
        self.setup_conn_ui(self.left_panel)
        self.setup_control_ui(self.left_panel)

        # --- 右側: 受信モニター ---
        self.right_panel = ctk.CTkFrame(self.main_container)
        self.right_panel.grid(row=0, column=1, padx=5, pady=5, sticky="nsew")
        self.right_panel.grid_rowconfigure(1, weight=1)
        self.right_panel.grid_columnconfigure(0, weight=1)

        self.setup_monitor_ui(self.right_panel)

    def setup_conn_ui(self, parent):
        frame = self.create_section_frame(parent, "Connection")
        
        # Serial Port
        ctk.CTkLabel(frame, text="Serial Port (Send Commands):").pack(anchor="w", padx=5)
        self.port_combo = ctk.CTkComboBox(frame, values=self.get_serial_ports())
        self.port_combo.pack(fill="x", padx=5, pady=2)
        
        # HID Device
        ctk.CTkLabel(frame, text="HID Device (Monitor Input):").pack(anchor="w", padx=5, pady=(5, 0))
        self.hid_combo = ctk.CTkComboBox(frame, values=self.get_hid_devices())
        self.hid_combo.pack(fill="x", padx=5, pady=2)

        self.conn_btn = ctk.CTkButton(frame, text="Connect", command=self.toggle_connection)
        self.conn_btn.pack(fill="x", padx=5, pady=10)

    def setup_control_ui(self, parent):
        frame = self.create_section_frame(parent, "HID Output Control (Send via Serial)")
        
        # Steer
        ctk.CTkLabel(frame, text="Steer (-32767 to 32767):").pack(anchor="w", padx=5)
        self.steer_slider = self.create_slider_with_label(frame, -32767, 32767, 0, self.update_and_send)
        
        # Accel
        ctk.CTkLabel(frame, text="Accel (-32767 to 32767):").pack(anchor="w", padx=5, pady=(5,0))
        self.accel_slider = self.create_slider_with_label(frame, -32767, 32767, -32767, self.update_and_send)
        
        # Brake
        ctk.CTkLabel(frame, text="Brake (-32767 to 32767):").pack(anchor="w", padx=5, pady=(5,0))
        self.brake_slider = self.create_slider_with_label(frame, -32767, 32767, -32767, self.update_and_send)

        # Buttons
        ctk.CTkLabel(frame, text="Buttons (1-16):").pack(anchor="w", padx=5, pady=(10,0))
        btn_grid = ctk.CTkFrame(frame, fg_color="transparent")
        btn_grid.pack(fill="x", padx=5, pady=5)
        
        self.btn_vars = []
        for i in range(16):
            var = tk.BooleanVar(value=False)
            chk = ctk.CTkCheckBox(btn_grid, text=f"{i+1}", variable=var, width=50, command=self.update_and_send)
            chk.grid(row=i//4, column=i%4, padx=5, pady=2, sticky="w")
            self.btn_vars.append(var)

    def setup_monitor_ui(self, parent):
        # リアルタイム数値表示
        mon_frame = self.create_section_frame(parent, "HID Input Monitor (Received from Device)")
        
        self.mon_steer = self.create_monitor_row(mon_frame, "Steer:")
        self.mon_accel = self.create_monitor_row(mon_frame, "Accel:")
        self.mon_brake = self.create_monitor_row(mon_frame, "Brake:")
        self.mon_btns  = self.create_monitor_row(mon_frame, "Buttons:")

        # ログエリア
        log_label = ctk.CTkLabel(parent, text="Serial Debug Log", font=ctk.CTkFont(size=14, weight="bold"))
        log_label.grid(row=2, column=0, padx=10, pady=(10, 5), sticky="w")

        self.log_box = ctk.CTkTextbox(parent, state="disabled", height=200)
        self.log_box.grid(row=3, column=0, padx=10, pady=5, sticky="nsew")

    def create_section_frame(self, parent, title):
        frame = ctk.CTkFrame(parent)
        frame.pack(fill="x", padx=5, pady=5)
        if isinstance(parent, ctk.CTkFrame) and parent is self.right_panel:
            frame.grid(row=0, column=0, padx=5, pady=5, sticky="ew")
        
        label = ctk.CTkLabel(frame, text=title, font=ctk.CTkFont(size=13, weight="bold"))
        label.pack(anchor="w", padx=5, pady=5)
        return frame

    def create_slider_with_label(self, parent, start, end, init, command):
        f = ctk.CTkFrame(parent, fg_color="transparent")
        f.pack(fill="x", padx=5, pady=2)
        
        slider = ctk.CTkSlider(f, from_=start, to=end, number_of_steps=end-start, 
                               command=lambda v: [label.configure(text=str(int(v))), command()])
        slider.set(init)
        slider.pack(side="left", fill="x", expand=True)
        
        label = ctk.CTkLabel(f, text=str(init), width=60)
        label.pack(side="right", padx=5)
        return slider

    def create_monitor_row(self, parent, label_text):
        f = ctk.CTkFrame(parent, fg_color="transparent")
        f.pack(fill="x", padx=10, pady=2)
        ctk.CTkLabel(f, text=label_text, width=80, anchor="w").pack(side="left")
        val_label = ctk.CTkLabel(f, text="0", font=ctk.CTkFont(family="Consolas", size=14, weight="bold"), text_color="#1f6aa5")
        val_label.pack(side="left", padx=10)
        return val_label

    # --- ロジック ---
    def get_serial_ports(self):
        return [p.device for p in serial.tools.list_ports.comports()]

    def get_hid_devices(self):
        devs = []
        for d in hid.enumerate():
            usage_page = d.get('usage_page', 0)
            usage = d.get('usage', 0)
            if usage_page == 0x01 and usage in [0x04, 0x05]:
                name = d.get('product_string') or "Unknown"
                path = d.get('path', b'').decode(errors='ignore')
                devs.append(f"{name} ({path})")
        
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
            p = self.port_combo.get()
            if p:
                self.serial_inst = serial.Serial(p, 115200, timeout=0.1)
                self.add_log(f"Serial connected: {p}")
            
            h = self.hid_combo.get()
            if "(" in h:
                path = h.split("(")[-1].strip(")")
                self.hid_device = hid.device()
                self.hid_device.open_path(path.encode())
                self.hid_device.set_nonblocking(True)
                self.add_log(f"HID connected: {h}")
            
            self.conn_btn.configure(text="Disconnect", fg_color="red")
        except Exception as e:
            self.add_log(f"Conn Error: {e}")
            self.disconnect()

    def disconnect(self):
        if self.serial_inst: self.serial_inst.close(); self.serial_inst = None
        if self.hid_device: self.hid_device.close(); self.hid_device = None
        self.conn_btn.configure(text="Connect", fg_color=["#3B8ED0", "#1F6AA5"])
        self.add_log("Disconnected.")

    def update_and_send(self, _=None):
        if not self.serial_inst: return
        
        s = int(self.steer_slider.get())
        a = int(self.accel_slider.get())
        b = int(self.brake_slider.get())
        
        btns = 0
        for i, var in enumerate(self.btn_vars):
            if var.get(): btns |= (1 << i)
        
        cmd = f"HID:S{s},A{a},B{b},BTN{btns}\n"
        try:
            self.serial_inst.write(cmd.encode())
        except:
            pass

    def serial_read_task(self):
        while self.running:
            if self.serial_inst and self.serial_inst.is_open:
                try:
                    l = self.serial_inst.readline().decode('utf-8', errors='ignore').strip()
                    if l: self.add_log(f"[DEV] {l}")
                except: pass
            time.sleep(0.01)

    def hid_read_task(self):
        while self.running:
            if self.hid_device:
                try:
                    # Report ID 1 (Gamepad) を想定
                    # 構造: [ID(1), Steer(2), Accel(2), Brake(2), Buttons(2)] = 9 bytes
                    d = self.hid_device.read(64)
                    if d:
                        # TinyUSB/hidapi の仕様により ID が含まれる場合と含まれない場合がある
                        # 今回の記述子では ID 1 を使用
                        if d[0] == 0x01 and len(d) >= 9:
                            s, a, b, btns = struct.unpack("<hhhH", bytes(d[1:9]))
                            self.after(0, self.update_monitor, s, a, b, btns)
                except: pass
            time.sleep(0.001)

    def update_monitor(self, s, a, b, btns):
        self.mon_steer.configure(text=f"{s}")
        self.mon_accel.configure(text=f"{a}")
        self.mon_brake.configure(text=f"{b}")
        self.mon_btns.configure(text=f"{btns:016b} ({btns})")

    def add_log(self, text):
        self.log_box.configure(state="normal")
        self.log_box.insert("end", text + "\n")
        self.log_box.see("end")
        self.log_box.configure(state="disabled")

    def on_closing(self):
        self.running = False
        self.disconnect()
        self.destroy()

if __name__ == "__main__":
    app = HIDTesterApp()
    app.protocol("WM_DELETE_WINDOW", app.on_closing)
    app.mainloop()
