/***************************************************************************
 * Copyright (C) GFZ Potsdam                                               *
 * All rights reserved.                                                    *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 ***************************************************************************/


#include "mainframe.h"
#include "tracewidget.h"
#include <plugin.h>

#include <seiscomp/io/archive/xmlarchive.h>
#include <seiscomp/messaging/status.h>
#include <seiscomp/gui/core/application.h>
#include <seiscomp/gui/core/xmlview.h>


using namespace Seiscomp::IO;
using namespace Seiscomp::Gui;


namespace Seiscomp {
namespace Gui {
namespace MessageMonitor {

namespace {

struct ParamMapper {
	enum DataType {
		Integer,
		Double,
		String
	};

	struct Entry {
		Entry() : dataType(String) {}
		Entry(const QString &n) : name(n), dataType(String) {}
		Entry(const QString &n, DataType dt) : name(n), dataType(dt) {}
		QString name;
		DataType dataType;
	};

	typedef QMap<QString, Entry>::iterator iterator;

	ParamMapper() {
		init(Client::Status::Tag(Client::Status::Programname).toString(), "prog");
		init(Client::Status::Tag(Client::Status::Clientname).toString(), "name");
		init(Client::Status::Tag(Client::Status::Hostname).toString(), "host");
		init(Client::Status::Tag(Client::Status::TotalMemory).toString(), "hmem", Integer);
		init(Client::Status::Tag(Client::Status::ClientMemoryUsage).toString(), "cmem", Integer);
		init(Client::Status::Tag(Client::Status::CPUUsage).toString(), "cpu", Double);
		init(Client::Status::Tag(Client::Status::MessageQueueSize).toString(), "q", Integer);
		init(Client::Status::Tag(Client::Status::ObjectCount).toString(), "oc", Integer);
		init(Client::Status::Tag(Client::Status::Uptime).toString(), "uptime");
		init(Client::Status::Tag(Client::Status::ResponseTime).toString(), "resp", Integer);
	}

	void init(const char *param, const char *name, DataType dt = String) {
		names[param] = Entry(name, dt);
	}

	const QString &map(const QString &param, DataType *dt = NULL) const {
		QMap<QString, Entry>::const_iterator it = names.find(param);
		if ( it == names.end() ) {
			if ( dt ) *dt = String;
			return param;
		}
		if ( dt ) *dt = it.value().dataType;
		return it.value().name;
	}


	QMap<QString, Entry> names;
};


ParamMapper paramMapper;


}


class PluginThread : public QThread {
	public:
		PluginThread(QWidget* widget)
		 : _widget(widget) {}

		void run() {
			if ( _widget != NULL ) {
				_widget->show();
				exec();
			}

			exit(0);
		}

	private:
		QWidget* _widget;
};



QString prettySize(double size) {
	static const char *names[] = {"bytes", "kB", "MB", "GB", "TB", NULL};

	int idx = 0;

	while ( size >= 1024 && names[idx+1] ) {
		size /= 1024;
		++idx;
	}

	if ( idx == 0 )
		return QString("%1 %2").arg((int)size).arg(names[idx]);

	//return QString("%1 %2").arg(size, 0, 'f', 3).arg(names[idx-1]);
	return QString("%1 %2").arg((int)size).arg(names[idx]);
}


QString prettyTimeSpan(const Seiscomp::Core::TimeSpan& span) {
	int days, hours, minutes, seconds;
	span.get(&days, &hours, &minutes, &seconds);

	if ( days > 0 )
		return QString("%1d%2h%3m%4s")
		        .arg(days)
		        .arg(hours, 2, 10, QChar('0'))
		        .arg(minutes, 2, 10, QChar('0'))
		        .arg(seconds, 2, 10, QChar('0'));
	else if ( hours > 0 )
		return QString("%1h%2m%3s")
		        .arg(hours, 2, 10, QChar('0'))
		        .arg(minutes, 2, 10, QChar('0'))
		        .arg(seconds, 2, 10, QChar('0'));
	else if ( minutes > 0 )
		return QString("%1m%2s")
		        .arg(minutes, 2, 10, QChar('0'))
		        .arg(seconds, 2, 10, QChar('0'));
	else
		return QString("%1s")
		        .arg(seconds);
}


class MsgItem : public QTreeWidgetItem {
	public:
		MsgItem(const QString& payload) : QTreeWidgetItem(), _payload(payload) {}
		MsgItem(QTreeWidget *parent, const QString& payload) : QTreeWidgetItem(parent), _payload(payload) {}

		const QString& payload() const { return _payload; }
		void  setPayload(const QString& payload) { _payload = payload; }

	private:
		QString _payload;
};


class ClientItem : public QTreeWidgetItem {
	public:
		ClientItem(QTreeWidget* parent) : QTreeWidgetItem(parent) { init(); }

		void init() {
			_lastReponse = Core::Time::UTC();
		}

		const Core::Time &lastResponse() const {
			return _lastReponse;
		}

		void setLastResponse(const Core::Time &t) {
			_lastReponse = t;
		}

		void setValue(QTreeWidgetItem *header, const QString &param, const QString &value) {
			ParamMapper::DataType dt;
			const QString &colName = paramMapper.map(param, &dt);
			for ( int i = 0; i < header->columnCount(); ++i ) {
				if ( header->text(i) == colName ) {
					if ( dt == ParamMapper::Double )
						setText(i, QString("%1").arg(value.toDouble(), 0, 'f', 1));
					else
						setText(i, value);
					setData(i, Qt::UserRole, (int)dt);
					setTextAlignment(i, Qt::AlignRight);
					return;
				}
			}
		}

		bool operator< (const QTreeWidgetItem &other) const {
			int sortCol = treeWidget()->sortColumn();
			ParamMapper::DataType dt = (ParamMapper::DataType)data(sortCol, Qt::UserRole).value<int>();
			switch ( dt ) {
				case ParamMapper::Double:
					return text(sortCol).toDouble() < other.text(sortCol).toDouble();
				case ParamMapper::Integer:
					return text(sortCol).toInt() < other.text(sortCol).toInt();
				default:
					break;
			}

			return QTreeWidgetItem::operator<(other);
		}

	private:
		Core::Time _lastReponse;
};


MainFrame::MainFrame() {
	_ui.setupUi(this);

	SCApp->setFilterCommandsEnabled(false);

	_ui.treeMessages->setColumnCount(5);
	QTreeWidgetItem* header = new QTreeWidgetItem();
	header->setText(0, tr("Type"));
	header->setText(1, tr("Sender"));
	header->setText(2, tr("Destination"));
	header->setText(3, tr("Size"));
	header->setText(4, tr("Time"));
	_ui.treeMessages->setHeaderItem(header);

	_ui.treeClients->setColumnCount(4);
	header = new QTreeWidgetItem();
	header->setText(0, tr("Name"));
	header->setText(1, tr("Type[-1]"));
	header->setText(2, tr("Destination[-1]"));
	header->setText(3, tr("Time[-1]"));
	_ui.treeClients->setHeaderItem(header);

	_ui.treeActiveClients->setColumnCount(10);
	_ui.treeActiveClients->setRootIsDecorated(false);
	QStringList infoHeader;
	infoHeader << paramMapper.map(Client::Status::Tag(Client::Status::Programname).toString());
	infoHeader << paramMapper.map(Client::Status::Tag(Client::Status::Clientname).toString());
	infoHeader << paramMapper.map(Client::Status::Tag(Client::Status::Hostname).toString());
	infoHeader << paramMapper.map(Client::Status::Tag(Client::Status::TotalMemory).toString());
	infoHeader << paramMapper.map(Client::Status::Tag(Client::Status::ClientMemoryUsage).toString());
	infoHeader << paramMapper.map(Client::Status::Tag(Client::Status::CPUUsage).toString());
	infoHeader << paramMapper.map(Client::Status::Tag(Client::Status::MessageQueueSize).toString());
	infoHeader << paramMapper.map(Client::Status::Tag(Client::Status::ObjectCount).toString());
	infoHeader << paramMapper.map(Client::Status::Tag(Client::Status::Uptime).toString());
	infoHeader << paramMapper.map(Client::Status::Tag(Client::Status::ResponseTime).toString());

	_ui.treeActiveClients->setHeaderLabels(infoHeader);

	_traceWidget = new TraceWidget(_ui.splitterStatistics);
	_traceWidget->setObjectName(QString::fromUtf8("traceWidget"));
	_ui.splitterStatistics->addWidget(_traceWidget);

	_traceWidget->setValueCount(300);
	_traceWidget->setFrameStyle(QFrame::Panel);
	_traceWidget->setFrameShadow(QFrame::Sunken);

	_messageView = NULL;

	_ui.menuSettings->addAction(_actionShowSettings);

	connect(SCApp, SIGNAL(changedConnection()),
	        this, SLOT(onConnectionChanged()));

	connect(SCApp, SIGNAL(messageSkipped(Seiscomp::Client::Packet*)),
	        this, SLOT(onMessageSkipped(Seiscomp::Client::Packet*)));

	connect(SCApp, SIGNAL(messageAvailable(Seiscomp::Core::Message*, Seiscomp::Client::Packet*)),
	        this, SLOT(onMessageReceived(Seiscomp::Core::Message*, Seiscomp::Client::Packet*)));

	connect(_ui.actionQuit, SIGNAL(triggered(bool)), this, SLOT(close()));
	connect(_ui.actionConsoleLog, SIGNAL(triggered(bool)), this, SLOT(onConsoleLogEnabled(bool)));
	connect(_ui.actionAutoOpenMessage, SIGNAL(triggered(bool)), this, SLOT(onAutoOpenMessage(bool)));
	connect(_ui.actionClearMessages, SIGNAL(triggered()), this, SLOT(clearMessages()));

	connect(_ui.buttonApply, SIGNAL(clicked(bool)), this, SLOT(updateUISettings()));
	connect(_ui.treeMessages, SIGNAL(itemActivated(QTreeWidgetItem*, int)), this, SLOT(onItemSelected(QTreeWidgetItem*, int)));
	connect(_ui.treeClients , SIGNAL(itemActivated(QTreeWidgetItem*, int)), this, SLOT(onItemSelected(QTreeWidgetItem*, int)));

	connect(_logger, SIGNAL(logReceived(const QString&, int, const QString&, int)),
	        this, SLOT(onLog(const QString&, int, const QString&, int)));

	QIntValidator* messageCountValidator = new QIntValidator(1, 50000, this);
	_ui.editMessageCount->setValidator(messageCountValidator);

	updateUISettings();

	connect(&_timer, SIGNAL(timeout()), this, SLOT(onTimer()));

	onConsoleLogEnabled(_ui.actionConsoleLog->isChecked());

	SCScheme.applyTabPosition(_ui.tabWidget);

	_timer.start(1000);

	clearStatistics();
	loadPlugins();
}


MainFrame::~MainFrame() {
	if ( _messageView )
		delete _messageView;
}


void MainFrame::closeEvent(QCloseEvent* e) {
	for ( PluginWidgetMap::iterator it = _pluginWidgets.begin();
	      it != _pluginWidgets.end(); ++it ) {
		it.key()->close();
	}
	QMainWindow::closeEvent(e);
}


void MainFrame::loadPlugins() {
	QDir pluginsDir((Environment::Instance()->shareDir() + "/plugins/" + SCApp->name()).c_str());

	foreach ( QString fileName, pluginsDir.entryList(QDir::Files) ) {
		std::cout << "checking "
			<< (const char*)pluginsDir.absoluteFilePath(fileName).toLatin1()
			<< "...";

		QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
		QObject *plugin = loader.instance();
		if ( plugin ) {
			Plugin* p = qobject_cast<Plugin*>(plugin);
			if ( p ) {
				std::cout << "loaded plugin " << p->name();
				_ui.menuPlugins->setEnabled(true);
				QAction* action = new QAction(p->name(), plugin);
				_pluginActions[p] = action;
				connect(action, SIGNAL(triggered(bool)), this, SLOT(onPluginOpen(bool)));
				_ui.menuPlugins->addAction(action);
			}
			else
				std::cout << "failed (no matching interface found)";
		}
		else
			std::cout << "failed";

		std::cout << std::endl;
	}
}


void MainFrame::clearStatistics() {
	_msgInInterval = 0;

	_msgReceived = 0;
	_bytesReceived = 0;

	_lastSender = "";
	_lastDestination = "";
	_lastType = "";

	_clientStats.clear();
	_groupStats.clear();

	_ui.treeClients->clear();
	_ui.treeMessages->clear();

	_ui.treeStats->clear();
	QTreeWidgetItem *item = new QTreeWidgetItem(_ui.treeStats);
	item->setText(0, tr("Senders"));
	item->setText(1, "");
	item->setText(2, "");
	_ui.treeStats->expandItem(item);

	item = new QTreeWidgetItem(_ui.treeStats);
	item->setText(0, tr("Targets"));
	item->setText(1, "");
	item->setText(2, "");
	_ui.treeStats->expandItem(item);

	item = new QTreeWidgetItem(_ui.treeStats);
	_ui.treeStats->expandItem(item);
	item->setText(0, tr("Overall"));
	item->setText(1, "");
	item->setText(2, "");

	item = new QTreeWidgetItem(item);
	item->setText(1, "0");
	item->setText(2, "0");

	QFont f(item->font(0));
	f.setBold(true);
	item->setFont(1, f);
	item->setFont(2, f);

	for ( int i = 0; i < _ui.treeStats->topLevelItemCount(); ++i )
		for ( int j = 0; j < _ui.treeStats->topLevelItem(i)->columnCount(); ++j ) {
			_ui.treeStats->topLevelItem(i)->setBackground(j, QColor(128,128,128));
			_ui.treeStats->topLevelItem(i)->setForeground(j, Qt::white);
		}

	_traceWidget->clear();
}


void MainFrame::onConsoleLogEnabled(bool e) {
	if ( e )
		_logger.subscribe(Seiscomp::Logging::getAll());
	else
		_logger.clear();
}


void MainFrame::onAutoOpenMessage(bool e) {
	if ( e && !_messageView ) {
		_messageView = new XMLView(NULL, Qt::Tool, false);
		_messageView->show();
	}
	else if ( !e && _messageView ) {
		delete _messageView;
		_messageView = NULL;
	}
}


void MainFrame::clearMessages() {
	_ui.treeMessages->clear();
}


void MainFrame::onPluginOpen(bool) {
	QAction *action = qobject_cast<QAction*>(sender());
    Plugin* plugin = qobject_cast<Plugin*>(action->parent());
	QWidget* widget = plugin->create(NULL, this);
	if ( widget != NULL ) {
		connect(widget, SIGNAL(destroyed(QObject*)), this, SLOT(onPluginClosed(QObject*)));
		_pluginWidgets[widget] = plugin;
		widget->show();
		action->setEnabled(false);
	}
}


void MainFrame::onPluginClosed(QObject* obj) {
	QMap<QWidget*, Plugin*>::iterator it1 = _pluginWidgets.find(qobject_cast<QWidget*>(obj));
	if ( it1 == _pluginWidgets.end() )
		return;

	Plugin* plugin = it1.value();
	_pluginWidgets.erase(it1);

	QMap<Plugin*, QAction*>::iterator it2 = _pluginActions.find(plugin);
	if ( it2 == _pluginActions.end() )
		return;

	QAction* action = it2.value();
	action->setEnabled(true);
}


/*
void MainFrame::onConnect() {
	if ( _connection != NULL ) {
		_ui.buttonConnect->setText("Connect");
		_ui.editUser->setEnabled(true);
		_ui.editServer->setEnabled(true);
		_ui.editPrimaryGroup->setEnabled(true);

		_ui.groupSubscriptions->setEnabled(false);
		_ui.listSubscriptions->clear();

		_connection->disconnect();
		_connection = NULL;

		_timer.stop();

		return;
	}

	QString user = _ui.editUser->text();
	QString host = _ui.editServer->text();

	if ( user.isEmpty() || host.isEmpty() ) {
		QMessageBox::information(this, tr(""),
		                         tr("Please enter a user- and servername."),
		                         QMessageBox::Ok);
		return;
	}

	_connection = Connection::Create(("4803@" + host).toAscii(), user.toAscii(), _ui.editPrimaryGroup->text().toAscii());
	if ( _connection == NULL ) {
		QMessageBox::information(this, tr(""),
		                         tr("The connection has not been established."),
		                         QMessageBox::Ok);
		return;
	}


	clearStatistics();
	_ui.groupSubscriptions->setEnabled(true);

	_ui.listSubscriptions->blockSignals(true);
	for ( int i = 0; i < _connection->groupCount(); ++i ) {
		QListWidgetItem* item = new QListWidgetItem(_connection->group(i), _ui.listSubscriptions);
		item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
		item->setCheckState(Qt::Checked);
		_connection->subscribe(_connection->group(i));
	}
	_ui.listSubscriptions->blockSignals(false);

	//_connection->listen(Connection::THREADED);
	_timer.start(_pollingInterval);

	_ui.buttonConnect->setText("Disconnect");
	_ui.editUser->setEnabled(false);
	_ui.editServer->setEnabled(false);
	_ui.editPrimaryGroup->setEnabled(false);
}
*/


int MainFrame::findRow(QTreeWidget* table, int column, const QString& text) {
	for ( int i = 0; i < table->topLevelItemCount(); ++i ) {
		QTreeWidgetItem* item = table->topLevelItem(i);
		if ( item && item->text(column) == text )
			return i;
	}

	return -1;
}


void MainFrame::updateUISettings() {
	_maxRows = _ui.editMessageCount->text().toInt();
	if ( _maxRows < 1 ) {
		_maxRows = 1;
		_ui.editMessageCount->setText(QString("%1").arg(_maxRows));
	}
}


void MainFrame::onItemSelected(QTreeWidgetItem * item, int) {
	MsgItem* msgitem = (MsgItem*)item;
	XMLView* viewer = new XMLView(this, Qt::Tool);
	viewer->show();
	viewer->setContent(msgitem->payload());
}


void MainFrame::addMessage(Seiscomp::Core::Message *msg, Seiscomp::Client::Packet *pkt) {
	std::stringbuf sb;
	XMLArchive ar;

	// use schema version of connection if available
	if ( SCApp->connection() != NULL )
		ar.setVersion(SCApp->connection()->schemaVersion());

	ar.create(&sb);
	ar << msg;
	ar.close();
	QTreeWidgetItem *item = new MsgItem(sb.str().c_str());

	item->setText(0, msg->className());
	if ( _lastType != msg->className() ) {
		_lastType = msg->className();
		QFont f(item->font(0));
		f.setBold(true);
		item->setFont(0, f);
	}

	item->setText(1, pkt?pkt->sender.c_str():"");
	if ( _lastSender != item->text(1) ) {
		_lastSender = item->text(1);
		QFont f(item->font(1));
		f.setBold(true);
		item->setFont(1, f);
	}

	item->setText(2, pkt?pkt->target.c_str():"");
	if ( _lastDestination != item->text(2) ) {
		_lastDestination = item->text(2);
		QFont f(item->font(2));
		f.setBold(true);
		item->setFont(2, f);
	}

	item->setText(3, QString("%1").arg(pkt?pkt->payload.size():-1));
	item->setText(4, QDateTime::currentDateTime().toString(Qt::ISODate));

	_ui.treeMessages->insertTopLevelItem(0, item);

	while ( _ui.treeMessages->topLevelItemCount() > _maxRows ) {
		item = _ui.treeMessages->topLevelItem(_maxRows);
		if ( !item ) break;
		if ( item == _ui.treeMessages->currentItem() )
			_ui.treeMessages->setCurrentItem(NULL);
		delete item;
	}

	// Update client statistic
	updateStats(_lastSender, _lastDestination, pkt->payload.size());

	if ( _messageView )
		_messageView->setContent(sb.str().c_str());
}


void MainFrame::updateStats(const QString &client, const QString &group, size_t msgSize) {
	if ( !msgSize ) return;

	StatEntry &clientStats = _clientStats[client];
	++clientStats.messageCount;
	clientStats.bytes += msgSize;

	if ( clientStats.item == NULL ) {
		clientStats.item = new QTreeWidgetItem();
		clientStats.item->setText(0, client);
		_ui.treeStats->topLevelItem(0)->addChild(clientStats.item);
	}
	clientStats.item->setText(1, QString("%1").arg(clientStats.messageCount));
	clientStats.item->setText(2, QString("%1").arg(prettySize(clientStats.bytes)));

	// Update group statistic
	StatEntry& groupStats = _groupStats[group];
	++groupStats.messageCount;
	groupStats.bytes += msgSize;
	if ( groupStats.item == NULL ) {
		groupStats.item = new QTreeWidgetItem();
		groupStats.item->setText(0, group);
		_ui.treeStats->topLevelItem(1)->addChild(groupStats.item);
	}
	groupStats.item->setText(1, QString("%1").arg(groupStats.messageCount));
	groupStats.item->setText(2, QString("%1").arg(prettySize(groupStats.bytes)));

	++_msgReceived;
	_bytesReceived += msgSize;

	QTreeWidgetItem *item = _ui.treeStats->topLevelItem(2)->child(0);
	item->setText(1, QString("%1").arg(_msgReceived));
	item->setText(2, QString("%1").arg(prettySize(_bytesReceived)));
}


void MainFrame::updateClient(Seiscomp::Core::Message* msg) {
	std::stringbuf sb;
	XMLArchive ar(&sb, false);
	ar << msg;
	ar.close();

	int row = findRow(_ui.treeClients, 0, _lastSender);

	QTreeWidgetItem* item;

	if ( row == -1 ) {
		item = new MsgItem(_ui.treeClients, sb.str().c_str());
		//item = new QTreeWidgetItem(_ui.treeClients);
		item->setText(0, _lastSender);
	}
	else {
		item = _ui.treeClients->topLevelItem(row);
		MsgItem *msgitem = (MsgItem*)item;
		msgitem->setPayload( sb.str().c_str() );
	}

	// Last type
	item->setText(1, msg->className());

	// Last destination
	item->setText(2, _lastDestination);

	// Last time
	item->setText(3, QDateTime::currentDateTime().toString(Qt::ISODate));
}

void MainFrame::updateView(Seiscomp::Core::Message* msg, Seiscomp::Client::Packet *pkt) {
	addMessage(msg, pkt);
	updateClient(msg);
}


void MainFrame::updateClient(const QString& name, const QString& upTime) {
	ClientItem* item = _clientMap[name];
	if ( item == NULL ) {
		item = new ClientItem(_ui.treeActiveClients);
		item->setText(0, name);
		_clientMap[name] = item;
	}
}


void MainFrame::onConnectionChanged() {
	_ui.treeActiveClients->setEnabled(true);
	if ( !Application::Instance()->connection() ||
	     !Application::Instance()->connection()->isConnected() ) {
		_timer.stop();
		_ui.treeActiveClients->clear();
		_clientMap.clear();
		_ui.treeActiveClients->clear();
	}
	else {
		_timer.start(1000);
		Application::Instance()->connection()->subscribe("STATUS_GROUP");
	}
}


void MainFrame::onMessageSkipped(Seiscomp::Client::Packet *pkt) {
	if ( pkt == NULL ) return;

	++_msgInInterval;
	updateStats(pkt->sender.c_str(), pkt->target.c_str(), pkt->payload.size());

	switch ( pkt->type ) {
		case Client::Packet::Status:
		{
			QTreeWidgetItem *header = _ui.treeActiveClients->headerItem();
			QString name = QString(pkt->subject.c_str());
			ClientItem *item = _clientMap.value(name);
			if ( !item ) {
				item = new ClientItem(_ui.treeActiveClients);
				_clientMap[name] = item;
				if ( header )
					item->setValue(header, Client::Status::Tag(Client::Status::Clientname).toString(), name);
			}

			QStringList info = QString(pkt->payload.c_str()).split('&');
			Core::Time now = Seiscomp::Core::Time::UTC();
			Seiscomp::Core::TimeSpan responseTime = now - item->lastResponse();
			item->setLastResponse(now);

			if ( header ) {
				item->setValue(header, Client::Status::Tag(Client::Status::ResponseTime).toString(), prettyTimeSpan(responseTime));

				foreach ( const QString &s, info ) {
					QStringList toks = s.split('=');
					if ( toks.size() != 2 ) continue;

					item->setValue(header, toks[0], toks[1]);
				}

			}

			for ( int i = 0; i < _ui.treeActiveClients->header()->count(); ++i )
				_ui.treeActiveClients->resizeColumnToContents(i);

			break;
		}

		case Client::Packet::Disconnected:
		{
			QString name = QString(pkt->subject.c_str());
			ClientItem *item = _clientMap.value(name);
			if ( item ) {
				_clientMap.remove(name);
				delete item;
			}
			break;
		}

		default:
			break;
	};
}


void MainFrame::onMessageReceived(Seiscomp::Core::Message* msg, Seiscomp::Client::Packet *pkt) {
	++_msgInInterval;
	updateView(msg, pkt);
	messageReceived(msg);
}


void MainFrame::onTimer() {
	setUpdatesEnabled(false);

	_traceWidget->addValue(_msgInInterval);
	_msgInInterval = 0;

	QTreeWidgetItem *header = _ui.treeActiveClients->headerItem();
	if ( header ) {
		for ( int i = 0; i < _ui.treeActiveClients->topLevelItemCount(); ++i ) {
			ClientItem *item = (ClientItem*)_ui.treeActiveClients->topLevelItem(i);
			Core::Time now = Seiscomp::Core::Time::UTC();
			Seiscomp::Core::TimeSpan responseTime = now - item->lastResponse();
			item->setValue(header, Client::Status::Tag(Client::Status::ResponseTime).toString(), prettyTimeSpan(responseTime));
		}
	}

	setUpdatesEnabled(true);
}


void MainFrame::onLog(const QString &/*channelName*/,
                      int level,
                      const QString &msg,
                      int time) {
	QDateTime dt;
	dt.setSecsSinceEpoch(time);

	_ui.textLog->setTextColor(QColor(0,0,0));
	_ui.textLog->insertPlainText("[" + dt.toString(Qt::ISODate) + "] ");

	switch ( (Seiscomp::Logging::LogLevel)level ) {
		case Logging::LL_ERROR:
			_ui.textLog->setTextColor(QColor(128,0,0));
			break;
		case Logging::LL_DEBUG:
			_ui.textLog->setTextColor(QColor(0,128,0));
			break;
		case Logging::LL_WARNING:
			_ui.textLog->setTextColor(QColor(128,128,128));
			break;
		case Logging::LL_INFO:
			_ui.textLog->setTextColor(QColor(0,0,128));
			break;
		default:
			_ui.textLog->setTextColor(QColor(0,0,0));
			break;
	};

	_ui.textLog->insertPlainText(QString(msg) + "\n");
}


}
}
}
