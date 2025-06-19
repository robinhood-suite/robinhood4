import os

CONFIG_DIR = "/etc/robinhood4.d/"  # Directory where configuration files are stored

def get_config_path(fs_name):
    """Returns the path of the configuration file for the specified filesystem."""
    config_file = f"{fs_name}.py"  # Each filesystem has a corresponding config file
    config_path = os.path.join(CONFIG_DIR, config_file)

    if not os.path.exists(config_path):
        raise FileNotFoundError(f"Configuration file for {fs_name} not found: {config_path}")

    return config_path

def load_config(fs_name):
    """Dynamically loads the configuration file and executes its contents."""
    config_path = get_config_path(fs_name)

    config = {}
    with open(config_path) as f:
        exec(f.read(), config)  # Executes the configuration file and extracts variables

    return config

