#include "stdafx.h"

Map* map = nullptr;// = new Map();
ini::INI world_edit_strings;
ini::INI world_edit_game_strings;
ini::INI world_edit_data;
WindowHandler window_handler;

slk::SLK units_slk;
slk::SLK units_meta_slk;
slk::SLK items_slk;
slk::SLK abilities_slk;
slk::SLK doodads_slk;
slk::SLK doodads_meta_slk;
slk::SLK destructibles_slk;
slk::SLK destructibles_meta_slk;

HiveWE::HiveWE(QWidget* parent) : QMainWindow(parent) {
	setAutoFillBackground(true);

	fs::path directory = find_warcraft_directory();
	while (!fs::exists(directory / "War3x.mpq")) {
		directory = QFileDialog::getExistingDirectory(this, u8"选择魔兽目录", "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks).toStdWString();
	}

	//fs::path directory = find_warcraft_directory();
	//while (!fs::exists(directory / "Data") || directory == "") {
	//	directory = QFileDialog::getExistingDirectory(this, "Select Warcraft Directory", "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks).toStdWString();
	//	if (directory == "") {
	//		exit(EXIT_SUCCESS);
	//	}
	//}
	QSettings settings;
	settings.setValue("warcraftDirectory", QString::fromStdString(directory.string()));
	hierarchy.warcraft_directory = directory;
	hierarchy.init();

	ui.setupUi(this);
	showMaximized();

	world_edit_strings.load("UI/WorldEditStrings.txt");
	world_edit_game_strings.load("UI/WorldEditGameStrings.txt");
	world_edit_data.load("UI/WorldEditData.txt");

	world_edit_data.substitute(world_edit_game_strings, "WorldEditStrings");
	world_edit_data.substitute(world_edit_strings, "WorldEditStrings");

	connect(ui.ribbon->undo, &QPushButton::clicked, [&]() { map->terrain_undo.undo(); });
	connect(ui.ribbon->redo, &QPushButton::clicked, [&]() { map->terrain_undo.redo(); });

	connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Z), this), &QShortcut::activated, ui.ribbon->undo, &QPushButton::click);
	connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Y), this), &QShortcut::activated, ui.ribbon->redo, &QPushButton::click);

	connect(ui.ribbon->units_visible, &QPushButton::toggled, [](bool checked) { map->render_units = checked; });
	connect(ui.ribbon->doodads_visible, &QPushButton::toggled, [](bool checked) { map->render_doodads = checked; });
	connect(ui.ribbon->pathing_visible, &QPushButton::toggled, [](bool checked) { map->render_pathing = checked; });
	connect(ui.ribbon->brush_visible, &QPushButton::toggled, [](bool checked) { map->render_brush = checked; });
	connect(ui.ribbon->lighting_visible, &QPushButton::toggled, [](bool checked) { map->render_lighting = checked; });
	connect(ui.ribbon->wireframe_visible, &QPushButton::toggled, [](bool checked) { map->render_wireframe = checked; });
	connect(ui.ribbon->debug_visible, &QPushButton::toggled, [](bool checked) { map->render_debug = checked; });
	connect(ui.ribbon->minimap_visible, &QPushButton::toggled, [&](bool checked) { (checked) ? minimap->show() : minimap->hide(); });

	connect(new QShortcut(Qt::Key_U, this), &QShortcut::activated, ui.ribbon->units_visible, &QPushButton::click);
	connect(new QShortcut(Qt::Key_D, this), &QShortcut::activated, ui.ribbon->doodads_visible, &QPushButton::click);
	connect(new QShortcut(Qt::Key_P, this), &QShortcut::activated, ui.ribbon->pathing_visible, &QPushButton::click);
	connect(new QShortcut(Qt::Key_L, this), &QShortcut::activated, ui.ribbon->lighting_visible, &QPushButton::click);
	connect(new QShortcut(Qt::Key_T, this), &QShortcut::activated, ui.ribbon->wireframe_visible, &QPushButton::click);
	connect(new QShortcut(Qt::Key_F3, this), &QShortcut::activated, ui.ribbon->debug_visible, &QPushButton::click);

	// Reload theme
	connect(new QShortcut(Qt::Key_F5, this), &QShortcut::activated, [&]() {
		QSettings settings;
		QFile file("Data/Themes/" + settings.value("theme").toString() + ".qss");
		file.open(QFile::ReadOnly);
		QString StyleSheet = QLatin1String(file.readAll());

		qApp->setStyleSheet(StyleSheet);
	});

	connect(ui.ribbon->reset_camera, &QPushButton::clicked, [&]() { camera->reset(); });
	connect(ui.ribbon->switch_camera, &QPushButton::clicked, this, &HiveWE::switch_camera);
	setAutoFillBackground(true);

	connect(new QShortcut(Qt::Key_F1, this), &QShortcut::activated, ui.ribbon->switch_camera, &QPushButton::click);
	connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C), this), &QShortcut::activated, ui.ribbon->reset_camera, &QPushButton::click);

	connect(ui.ribbon->import_heightmap, &QPushButton::clicked, this, &HiveWE::import_heightmap);

	//connect(ui.ribbon->new_map, &QAction::triggered, this, &HiveWE::load);
	connect(ui.ribbon->open_map, &QPushButton::clicked, this, &HiveWE::load);
	connect(ui.ribbon->save_map, &QPushButton::clicked, [&]() { map->save(map->filesystem_path); });
	connect(ui.ribbon->save_map_as, &QPushButton::clicked, this, &HiveWE::save_as);
	connect(ui.ribbon->test_map, &QPushButton::clicked, [&]() { map->play_test(); });
	connect(ui.ribbon->settings, &QPushButton::clicked, [&]() { new SettingsEditor(this); });
	connect(ui.ribbon->switch_warcraft, &QPushButton::clicked, this, &HiveWE::switch_warcraft);
	connect(ui.ribbon->exit, &QPushButton::clicked, [&]() { QApplication::exit(); });

	connect(ui.ribbon->change_tileset, &QRibbonButton::clicked, [this]() { new TileSetter(this); });
	connect(ui.ribbon->change_tile_pathing, &QRibbonButton::clicked, [this]() { new TilePather(this); });

	connect(ui.ribbon->map_description, &QRibbonButton::clicked, [&]() { (new MapInfoEditor(this))->ui.tabs->setCurrentIndex(0); });
	connect(ui.ribbon->map_loading_screen, &QRibbonButton::clicked, [&]() { (new MapInfoEditor(this))->ui.tabs->setCurrentIndex(1); });
	connect(ui.ribbon->map_options, &QRibbonButton::clicked, [&]() { (new MapInfoEditor(this))->ui.tabs->setCurrentIndex(2); });
	//connect(ui, &QAction::triggered, [&]() { (new MapInfoEditor(this))->ui.tabs->setCurrentIndex(3); });

	connect(ui.ribbon->terrain_palette, &QRibbonButton::clicked, [this]() {
		auto palette = new TerrainPalette(this);
		palette->move(width() - palette->width() - 10, ui.widget->y() + 29);
		connect(palette, &TerrainPalette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(palette, &DoodadPalette::finished, this, &HiveWE::remove_custom_tab);
	});
	connect(ui.ribbon->doodad_palette, &QRibbonButton::clicked, [this]() {
		auto palette = new DoodadPalette(this);
		palette->move(width() - palette->width() - 10, ui.widget->y() + 29);
		connect(palette, &Palette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
		connect(this, &HiveWE::palette_changed, palette, &Palette::deactivate);
		connect(palette, &Palette::finished, [&]() {
			remove_custom_tab();
			disconnect(this, &HiveWE::palette_changed, palette, &Palette::deactivate);
		});
	});
	connect(ui.ribbon->pathing_palette, &QRibbonButton::clicked, [this]() {
		auto palette = new PathingPallete(this);
		palette->move(width() - palette->width() - 10, ui.widget->y() + 29);
		connect(this, &HiveWE::tileset_changed, [palette]() {
			palette->close();
		});
	});
	setAutoFillBackground(true);

	connect(ui.ribbon->import_manager, &QRibbonButton::clicked, []() { window_handler.create_or_raise<ImportManager>(); });
	connect(ui.ribbon->trigger_editor, &QRibbonButton::clicked, [this]() { 
		auto editor = window_handler.create_or_raise<TriggerEditor>();
		connect(this, &HiveWE::saving_initiated, editor, &TriggerEditor::save_changes, Qt::UniqueConnection);
	});

	minimap->setParent(ui.widget);
	minimap->move(10, 10);
	minimap->show();

	// Temporary Temporary
	//QTimer::singleShot(5, [this]() {
	//	auto editor = window_handler.create_or_raise<TriggerEditor>();
	//	connect(this, &HiveWE::saving_initiated, editor, &TriggerEditor::save_changes, Qt::UniqueConnection);
	//});

	connect(minimap, &Minimap::clicked, [](QPointF location) { camera->position = { location.x() * map->terrain.width, (1.0 - location.y()) * map->terrain.height ,camera->position.z };  });
	map = new Map();
	connect(&map->terrain, &Terrain::minimap_changed, minimap, &Minimap::set_minimap);
	//map->load("C:\\Users\\User\\stack\\Projects\\MCFC\\7.3\\Backup\\MCFC 7.3.w3x");
	fs::remove(fs::temp_directory_path() / "temp_map.w3x");
	fs::copy_file("Data/Test.w3x", fs::temp_directory_path() / "temp_map.w3x");
	map->load(fs::temp_directory_path() / "temp_map.w3x");
	

	//QTimer::singleShot(50, [this]() {
	//	auto palette = new TerrainPalette(this);
	//	palette->move(width() - palette->width() - 10, ui.widget->y() + 29);
	//	connect(palette, &TerrainPalette::ribbon_tab_requested, this, &HiveWE::set_current_custom_tab);
	//	connect(palette, &DoodadPalette::finished, this, &HiveWE::remove_custom_tab);
	//});
}

void HiveWE::load() {
	QSettings settings;

	QString file_name = QFileDialog::getOpenFileName(this, "Open File",
		settings.value("openDirectory", QDir::current().path()).toString(),
		"Warcraft III Scenario (*.w3m *.w3x)");

	if (file_name != "") {
		settings.setValue("openDirectory", file_name);

		// Try opening the archive
		HANDLE handle;
		bool success = SFileOpenArchive(fs::path(file_name.toStdString()).c_str(), 0, 0, &handle);
		if (!success) {
			QMessageBox::information(this, "Opening map failed", "Opening the map archive failed. It might be opened in another program.");
			return;
		}
		SFileCloseArchive(handle);

		delete map;
		map = new Map();

		connect(&map->terrain, &Terrain::minimap_changed, minimap, &Minimap::set_minimap);

		//ui.widget->makeCurrent();
		map->load(file_name.toStdString());
	}
}

void HiveWE::save_as() {
	QSettings settings;
	const QString directory = settings.value("openDirectory", QDir::current().path()).toString() + "/" + QString::fromStdString(map->filesystem_path.filename().string());

	QString file_name = QFileDialog::getSaveFileName(this, "Save File",
		directory,
		"Warcraft III Scenario (*.w3x)");

	if (file_name != "") {
		emit saving_initiated();
		map->save(file_name.toStdString());
	}
}

void HiveWE::closeEvent(QCloseEvent* event) {


	QMessageBox buttonBox(QMessageBox::Question, "HiveWE", u8"你要退出吗");
	buttonBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
	buttonBox.setButtonText(QMessageBox::Yes, u8"确定");
	buttonBox.setButtonText(QMessageBox::Cancel, u8"取消");

	const int choice = buttonBox.exec();
	if (choice == QMessageBox::Yes) {
		event->accept();
	} else {
		event->ignore();
	}
}

void HiveWE::switch_warcraft() {
	//fs::path directory;
	//do {
	//	directory = QFileDialog::getExistingDirectory(this, "Select Warcraft Directory", "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks).toStdWString();
	//	if (directory == "")
	//		directory = hierarchy.warcraft_directory;
	//} while (!fs::exists(directory / "Data"));

	fs::path directory = find_warcraft_directory();
	while (!fs::exists(directory / "War3x.mpq")) {
		directory = QFileDialog::getExistingDirectory(this, u8"选择魔兽目录", "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks).toStdWString();
	}

	QSettings settings;
	settings.setValue("warcraftDirectory", QString::fromStdString(directory.string()));

	if (directory != hierarchy.warcraft_directory) {
		hierarchy.game_data.close();
		hierarchy.warcraft_directory = directory;
		hierarchy.init();
	}
}

void HiveWE::switch_camera() {
	if (camera == &ui.widget->tps_camera) {
		ui.widget->fps_camera.horizontal_angle = ui.widget->tps_camera.horizontal_angle;
		ui.widget->fps_camera.vertical_angle = ui.widget->tps_camera.vertical_angle;

		ui.widget->fps_camera.position = ui.widget->tps_camera.position;
		camera = &ui.widget->fps_camera;
		ui.actionDoodads->setEnabled(false);
	} else {
		ui.widget->tps_camera.horizontal_angle = ui.widget->fps_camera.horizontal_angle;
		ui.widget->tps_camera.vertical_angle = ui.widget->fps_camera.vertical_angle;

		ui.widget->tps_camera.position = ui.widget->fps_camera.position;
		camera = &ui.widget->tps_camera;
		ui.actionDoodads->setEnabled(true);
	}
	camera->update(0);
}

// ToDo move to terrain class?
void HiveWE::import_heightmap() {
	QMessageBox::information(this, "Heightmap information", "Will read the red channel and map this onto the range -16 to +16");
	QSettings settings;
	const QString directory = settings.value("openDirectory", QDir::current().path()).toString() + "/" + QString::fromStdString(map->filesystem_path.filename().string());

	QString file_name = QFileDialog::getOpenFileName(this, "Open Heightmap Image", directory);

	if (file_name == "") {
		return;
	}

	int width;
	int height;
	int channels;
	uint8_t* image_data = SOIL_load_image(file_name.toStdString().c_str(), &width, &height, &channels, SOIL_LOAD_AUTO);

	if (width != map->terrain.width || height != map->terrain.height) {
		QMessageBox::warning(this, "Incorrect Image Size", QString("Image Size: %1x%2 does not match terrain size: %3x%4").arg(QString::number(width), QString::number(height), QString::number(map->terrain.width), QString::number(map->terrain.height)));
		return;
	}

	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			map->terrain.corners[i][j].height = (image_data[((height - 1 - j) * width + i) * channels] - 128) / 16.f;
		}
	}

	map->terrain.update_ground_heights({ 0, 0, width, height });
}

void HiveWE::set_current_custom_tab(QRibbonTab* tab, QString name) {
	if (current_custom_tab == tab) {
		return;
	}

	if (current_custom_tab != nullptr) {
		emit palette_changed(tab);
	}

	remove_custom_tab();
	current_custom_tab = tab;
	ui.ribbon->addTab(tab, name);
	ui.ribbon->setCurrentIndex(ui.ribbon->count() - 1);
}

void HiveWE::remove_custom_tab() {
	for (int i = 0; i < ui.ribbon->count(); i++) {
		if (ui.ribbon->widget(i) == current_custom_tab) {
			ui.ribbon->removeTab(i);
			current_custom_tab = nullptr;
			return;
		}
	}
}