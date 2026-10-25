# input: named third-party component; output: pinned archive metadata; pos: system source-of-truth.
KERNEL_COMMIT = '7ac2585f384573da8cbdcc987a3503e2e6ff1fb4'
SOURCES = {
    'kernel': {
        'archive': 'linux-source.tar.gz',
        'url': 'https://codeload.github.com/katadelos/linux-3.0.35-lab126/tar.gz/' + KERNEL_COMMIT,
        'sha256': 'bc1ae0227b1c636ebf2d2d0c46bb93f2028cf55c06884fe9d33f6153714877a5',
    },
    'busybox': {
        'archive': 'busybox-1.36.1.tar.bz2',
        'url': 'https://busybox.net/downloads/busybox-1.36.1.tar.bz2',
        'sha256': 'b8cc24c9574d809e7279c3be349795c5d5ceb6fdf19ca709f80cde50e47de314',
    },
    'barebox': {
        'archive': 'barebox-2026.09.0.tar.bz2',
        'url': 'https://www.barebox.org/download/barebox-2026.09.0.tar.bz2',
        'sha256': '491b504c8a79b5a4e30c419b824916b7329bad8abfe34987ba066bafa1fbcfc5',
    },
    'dropbear': {
        'archive': 'dropbear-2026.94.tar.bz2',
        'url': 'https://dropbear.nl/mirror/releases/dropbear-2026.94.tar.bz2',
        'sha256': 'e098034a843699200c8c977a991fff73159735bf795d5f72ef672c41a6b1ae81',
    },
    'wpa_supplicant': {
        'archive': 'wpa_supplicant-2.12.tar.gz',
        'url': 'https://w1.fi/releases/wpa_supplicant-2.12.tar.gz',
        'sha256': '08e23937e16d0155e55cab2b51f51fbe10d80a1aa91c4e15442645059b737ef6',
    },
    'libnl': {
        'archive': 'libnl-3.12.0.tar.gz',
        'url': 'https://github.com/thom311/libnl/releases/download/libnl3_12_0/libnl-3.12.0.tar.gz',
        'sha256': 'fc51ca7196f1a3f5fdf6ffd3864b50f4f9c02333be28be4eeca057e103c0dd18',
    },
}
