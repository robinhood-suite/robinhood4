# Installer le script Python uniquement dans le sous-dossier                     
install_data('mydaemon.py', install_dir: '/usr/local/bin')

# Assurer qu'il soit exécutable
custom_target('set-executable-permissions',
    output: ['set-exec-permissions.log'],  # Ajout d'un output valide
    command: ['chmod', '+x', '/usr/local/bin/mydaemon.py'])  # Retrait de install: true

# Installer le fichier systemd
install_data('mydaemon.service', install_dir: '/etc/systemd/system')

# Activer le service au démarrage
custom_target('enable-systemd-service',
    output: ['enable-systemd.log'],  # Ajout d'un output valide
    command: ['systemctl', 'enable', 'mydaemon.service'])  # Retrait de install: true

# Démarrer le service après installation
custom_target('start-systemd-service',
    output: ['start-systemd.log'],  # Ajout d'un output valide
    command: ['systemctl', 'start', 'mydaemon.service'])  # Retrait de install: true

