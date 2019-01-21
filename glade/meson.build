vte_conf = configuration_data()
vte_conf.set('VTE_API_VERSION', vte_api_version)
vte_conf.set('VERSION', vte_version)

vte_glade = configure_file(
  input: 'vte.xml.in',
  output: vte_api_name + '.xml',
  configuration: vte_conf,
  install: true,
  install_dir: glade_catalogdir,
)

run_target(
  'validate',
  command: [find_program('xmllint'), '--dtdvalid', glade_catalog_dtd, '--noout', vte_glade]
)

icon_sizes = [
  '16x16',
  '22x22',
]

icon = 'widget-vte-terminal.png'

sources = []
renames = []
foreach icon_size: icon_sizes
  sources += ['_'.join(['hicolor', 'actions', icon_size, icon])]
  renames += [icon_size / 'actions' / icon]
endforeach

install_data(
  sources,
  rename: renames,
  install_dir: glade_pixmapdir / 'hicolor',
)
