import yaml
import os
import datetime
import threading

class Config:
    def __init__(self, file_path):
        self.file_path = file_path
        self.conf = {}
        self.lock = threading.Lock()
        self.load_config()

    def load_config(self):
        """Load or create conf yaml"""
        if not os.path.exists(self.file_path):
            print("Initializing new config file")
            open(self.file_path, 'w').close()
            return {}
        
        with self.lock and open(self.file_path, 'r') as file:
            try:
                self.conf = yaml.safe_load(file) or {}
            except yaml.YAMLError as e:
                print(f"Error loading YAML file: {e}")
                return {}
            
        self.t_last_edit = self.get_last_conf_edit_time()
        print(f"Last edit time of the config file: {self.t_last_edit}")
        return self.conf
    
    def get_last_conf_edit_time(self):
        """Get the last edit time of the config file."""
        if os.path.exists(self.file_path):
            return datetime.datetime.fromtimestamp(os.path.getmtime(self.file_path)).replace(microsecond=0)
        else:
            return datetime.datetime.now().replace(microsecond=0)
        
    def save_config(self):
        """Save the latest config"""

        #print(f"Last edit time of the config file: {self.t_last_edit} vs {self.get_last_conf_edit_time()}")
        #if self.t_last_edit and self.t_last_edit < self.get_last_conf_edit_time():
        #    print("Config has been modified, merging ...")
        #    self.merge_config()

        with open(self.file_path, 'w') as file:
            try:
                yaml.safe_dump(self.conf, file)
            except yaml.YAMLError as e:
                print(f"Error saving YAML file: {e}")

        self.t_last_edit = self.get_last_conf_edit_time()

        #print("Latest config:", self.conf)

    def merge_config(self):
        # this functionality isn't working. The file mtime keeps changing....

        """Merge the current in-memory config with the file on disk."""
        with open(self.file_path, 'r') as file:
            try:
                file_conf = yaml.safe_load(file) or {}
            except yaml.YAMLError as e:
                print(f"Error loading YAML file during merge: {e}")
                return

        with self.lock:
            # Merge the file config into the in-memory config
            for key, value in file_conf.items():
                if key not in self.conf:
                    self.conf[key] = value
                elif isinstance(value, dict) and isinstance(self.conf[key], dict):
                    self.conf[key].update(value)

    def update_config(self, new_data):
        """Update the YAML configuration file with new data."""
        #config = self.load_config()
        #config.update(new_data)
        #self.save_config(config)

    def touch(self, name, ip, mac):
        """If the MAC isn't in config, add it coordinates 0,0.
        If it is, just update the type and ip."""
        # reload just in case it was edited by hand
        # print(f"Touching {name} {ip} {mac}")

        need_save = False

        with self.lock:
            if mac not in self.conf:
                self.conf[mac] = {}
                self.conf[mac]["coords"] = {"X": 0, "Y": 0}
                need_save = True

            if self.conf[mac].get("type") != name:
                self.conf[mac]["type"] = name
                #self.conf[mac]["ip"] = ip
                need_save = True

            if need_save:
                self.save_config()

    def get_coords(self, mac):
        """Get the coordinates of a device by its MAC address."""
        with self.lock:
            if mac in self.conf:
                return self.conf[mac].get("coords", None)
    
        return None