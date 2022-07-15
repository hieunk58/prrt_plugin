plugin_c_args = ['-DHAVE_CONFIG_H']

plugin_sources = [
  'gstprrtsrc.c'
  ]

gstvideoplugin = library('gst_prrt_plugin',
  plugin_sources,
  c_args: plugin_c_args,
  dependencies : [gst_dep],
  install : true,
  install_dir : plugins_install_dir,
)
