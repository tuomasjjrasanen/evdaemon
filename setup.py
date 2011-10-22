# -*- coding: utf-8 -*-

from distutils.core import setup

setup(name="evdaemon-admin",
      version="0.1.2",
      description="Tools for evdaemon administration.",
      long_description="Tools for evdaemon administration.",
      author="Tuomas Jorma Juhani Räsänen",
      author_email="tuomasjjrasanen@tjjr.fi",
      url="http://tjjr.fi/software/evdaemon/",
      package_dir={
        "evdaemon": "src/lib",
        },
      scripts=[
        "src/bin/evdaemon-admin-gtk",
        ],
      data_files=[("bin",
                   ["src/bin/evdaemon-admin-gtk.xml"]),
                  ("/usr/share/applications",
                   ["share/applications/evdaemon-admin.desktop"]),
                  ("/usr/share/pixmaps",
                   ["share/pixmaps/evdaemon-admin.xpm"]),
                  ],
      packages=["evdaemon"],
      )
