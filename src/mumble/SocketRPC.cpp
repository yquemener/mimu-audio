// Copyright 2005-2020 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "SocketRPC.h"

#include "Channel.h"
#include "ClientUser.h"
#include "MainWindow.h"
#include "ServerHandler.h"

#include <QtCore/QProcessEnvironment>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QLocalServer>
#include <QtXml/QDomDocument>

// We define a global macro called 'g'. This can lead to issues when included code uses 'g' as a type or parameter name (like protobuf 3.7 does). As such, for now, we have to make this our last include.
#include "Global.h"

SocketRPCClient::SocketRPCClient(QLocalSocket *s, QObject *p) : QObject(p), qlsSocket(s), qbBuffer(nullptr) {
	qlsSocket->setParent(this);

	connect(qlsSocket, SIGNAL(disconnected()), this, SLOT(disconnected()));
	connect(qlsSocket, SIGNAL(error(QLocalSocket::LocalSocketError)), this, SLOT(error(QLocalSocket::LocalSocketError)));
	connect(qlsSocket, SIGNAL(readyRead()), this, SLOT(readyRead()));

	qxsrReader.setDevice(qlsSocket);
	qxswWriter.setAutoFormatting(true);

	qbBuffer = new QBuffer(&qbaOutput, this);
	qbBuffer->open(QIODevice::WriteOnly);
	qxswWriter.setDevice(qbBuffer);
}

void SocketRPCClient::disconnected() {
	deleteLater();
}

void SocketRPCClient::error(QLocalSocket::LocalSocketError) {
}

void SocketRPCClient::readyRead() {
    char buf[256];
    int read = qlsSocket->read(buf, 255);
    buf[read]='\0';
    printf("Received: %s\n", buf);

    size_t pos = 0;
    std::string token;
    std::string cmd(buf);
    std::vector<std::string> tokens;
    while ((pos = cmd.find(" ")) != std::string::npos) {
        token = cmd.substr(0, pos);
        std::cout << token << std::endl;
        tokens.push_back(token);
        cmd.erase(0, pos + 1);
    }
    tokens.push_back(cmd);
    if(tokens.size()==0) return;
    if(tokens[0].compare("show")==0){
        g.mw->activateWindow();
        g.mw->show();
        g.mw->setFocus();
        g.mw->qteChat->setFocus();
        g.mw->qteChat->grabKeyboard();
    }else if(tokens[0].compare("hide")==0){
        g.mw->hide();
    }else if(tokens[0].compare("quit")==0){
        qApp->exit(0);
    }else if(tokens[0].compare("togglevisible")==0){
        if(g.mw->isVisible()){
            g.mw->hide();
        }
        else{
            g.mw->activateWindow();
            g.mw->show();
            g.mw->setFocus();
            g.mw->qteChat->setFocus();
            g.mw->qteChat->grabKeyboard();
        }
    }else if(tokens[0].compare("geom")==0){
        if(tokens.size()<5)
            return;
        int x = std::stoi(tokens[1]);
        int y = std::stoi(tokens[2]);
        g.mw->move(x,y);
    }


    /*
	forever {
		switch (qxsrReader.readNext()) {
			case QXmlStreamReader::Invalid: {
					if (qxsrReader.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
						qWarning() << "Malformed" << qxsrReader.error();
						qlsSocket->abort();
					}
					return;
				}
				break;
			case QXmlStreamReader::EndDocument: {
					qxswWriter.writeCurrentToken(qxsrReader);

					processXml();

					qxsrReader.clear();
					qxsrReader.setDevice(qlsSocket);

					qxswWriter.setDevice(nullptr);
					delete qbBuffer;
					qbaOutput = QByteArray();
					qbBuffer = new QBuffer(&qbaOutput, this);
					qbBuffer->open(QIODevice::WriteOnly);
					qxswWriter.setDevice(qbBuffer);
				}
				break;
			default:
				qxswWriter.writeCurrentToken(qxsrReader);
				break;
		}
    }*/
}

void SocketRPCClient::processXml() {
	QDomDocument qdd;
	qdd.setContent(qbaOutput, false);

	QDomElement request = qdd.firstChildElement();

	if (! request.isNull()) {
		bool ack = false;
		QMap<QString, QVariant> qmRequest;
		QMap<QString, QVariant> qmReply;
		QMap<QString, QVariant>::const_iterator iter;

		QDomNamedNodeMap attributes = request.attributes();
		for (int i=0;i<attributes.count();++i) {
			QDomAttr attr = attributes.item(i).toAttr();
			qmRequest.insert(attr.name(), attr.value());
		}
		QDomNodeList childNodes = request.childNodes();
		for (int i=0;i<childNodes.count();++i) {
			QDomElement child = childNodes.item(i).toElement();
			if (! child.isNull())
				qmRequest.insert(child.nodeName(), child.text());
		}

		iter = qmRequest.find(QLatin1String("reqid"));
		if (iter != qmRequest.constEnd())
			qmReply.insert(iter.key(), iter.value());

        if (request.nodeName() == QLatin1String("focus")) {
            g.mw->show();
            g.mw->raise();
            g.mw->activateWindow();

            ack = true;
        } else if (request.nodeName() == QLatin1String("self")) {
            iter = qmRequest.find(QLatin1String("mute"));
            if (iter != qmRequest.constEnd()) {
                bool set = iter.value().toBool();
                if (set != g.s.bMute) {
                    g.mw->qaAudioMute->setChecked(! set);
                    g.mw->qaAudioMute->trigger();
                }
            }
            iter = qmRequest.find(QLatin1String("hide"));
            if (iter != qmRequest.constEnd()) {
                 g.mw->hide();
            }
            iter = qmRequest.find(QLatin1String("show"));
            if (iter != qmRequest.constEnd()) {
                 //g.mw->setWindowFlags(Qt::WindowStaysOnTopHint);
                 g.mw->activateWindow();
                 g.mw->show();
                 g.mw->setFocus();
                 g.mw->qteChat->setFocus();
                 g.mw->qteChat->grabKeyboard();
            }
            iter = qmRequest.find(QLatin1String("unmute"));
            if (iter != qmRequest.constEnd()) {
                bool set = iter.value().toBool();
                if (set == g.s.bMute) {
                    g.mw->qaAudioMute->setChecked(set);
                    g.mw->qaAudioMute->trigger();
                }
            }
            iter = qmRequest.find(QLatin1String("geom"));
            if (iter != qmRequest.constEnd()) {
                iter = qmRequest.find(QLatin1String("geom"));
                bool set = iter.value().toBool();
                if (set == g.s.bMute) {
                    g.mw->qaAudioMute->setChecked(set);
                    g.mw->qaAudioMute->trigger();
                }
            }
            iter = qmRequest.find(QLatin1String("togglemute"));
			if (iter != qmRequest.constEnd()) {
				bool set = iter.value().toBool();
				if (set == g.s.bMute) {
					g.mw->qaAudioMute->setChecked(set);
					g.mw->qaAudioMute->trigger();
				} else {
					g.mw->qaAudioMute->setChecked(! set);
					g.mw->qaAudioMute->trigger();
				}
			}
			iter = qmRequest.find(QLatin1String("deaf"));
			if (iter != qmRequest.constEnd()) {
				bool set = iter.value().toBool();
				if (set != g.s.bDeaf) {
					g.mw->qaAudioDeaf->setChecked(! set);
					g.mw->qaAudioDeaf->trigger();
				}
			}
			iter = qmRequest.find(QLatin1String("undeaf"));
			if (iter != qmRequest.constEnd()) {
				bool set = iter.value().toBool();
				if (set == g.s.bDeaf) {
					g.mw->qaAudioDeaf->setChecked(set);
					g.mw->qaAudioDeaf->trigger();
				}
			}
			iter = qmRequest.find(QLatin1String("toggledeaf"));
			if (iter != qmRequest.constEnd()) {
				bool set = iter.value().toBool();
				if (set == g.s.bDeaf) {
					g.mw->qaAudioDeaf->setChecked(set);
					g.mw->qaAudioDeaf->trigger();
				} else {
					g.mw->qaAudioDeaf->setChecked(! set);
					g.mw->qaAudioDeaf->trigger();
				}
			}
			ack = true;
		} else if (request.nodeName() == QLatin1String("url")) {
			if (g.sh && g.sh->isRunning() && g.uiSession) {
				QString host, user, pw;
				unsigned short port;
				QUrl u;

				g.sh->getConnectionInfo(host, port, user, pw);
				u.setScheme(QLatin1String("mumble"));
				u.setHost(host);
				u.setPort(port);
				u.setUserName(user);

				QUrlQuery query;
				query.addQueryItem(QLatin1String("version"), QLatin1String("1.2.0"));
				u.setQuery(query);

				QStringList path;
				Channel *c = ClientUser::get(g.uiSession)->cChannel;
				while (c->cParent) {
					path.prepend(c->qsName);
					c = c->cParent;
				}
				u.setPath(path.join(QLatin1String("/")));
				qmReply.insert(QLatin1String("href"), u);
			}

			iter = qmRequest.find(QLatin1String("href"));
			if (iter != qmRequest.constEnd()) {
				QUrl u = iter.value().toUrl();
				if (u.isValid() && u.scheme() == QLatin1String("mumble")) {
					OpenURLEvent *oue = new OpenURLEvent(u);
					qApp->postEvent(g.mw, oue);
					ack = true;
				}
			} else {
				ack = true;
			}
		}

		QDomDocument replydoc;
		QDomElement reply = replydoc.createElement(QLatin1String("reply"));

		qmReply.insert(QLatin1String("succeeded"), ack);

		for (iter = qmReply.constBegin(); iter != qmReply.constEnd(); ++iter) {
			QDomElement elem = replydoc.createElement(iter.key());
			QDomText text = replydoc.createTextNode(iter.value().toString());
			elem.appendChild(text);
			reply.appendChild(elem);
		}

		replydoc.appendChild(reply);

		qlsSocket->write(replydoc.toByteArray());
	}
}

SocketRPC::SocketRPC(const QString &basename, QObject *p) : QObject(p) {
	qlsServer = new QLocalServer(this);

	QString pipepath;

#ifdef Q_OS_WIN
	pipepath = basename;
#else
	{
		QString xdgRuntimePath = QProcessEnvironment::systemEnvironment().value(QLatin1String("XDG_RUNTIME_DIR"));
		QDir xdgRuntimeDir = QDir(xdgRuntimePath);

		if (! xdgRuntimePath.isNull() && xdgRuntimeDir.exists()) {
			pipepath = xdgRuntimeDir.absoluteFilePath(basename + QLatin1String("Socket"));
		} else {
			pipepath = QDir::home().absoluteFilePath(QLatin1String(".") + basename + QLatin1String("Socket"));
		}
	}

	{
		QFile f(pipepath);
		if (f.exists()) {
			qWarning() << "SocketRPC: Removing old socket on" << pipepath;
			f.remove();
		}
	}
#endif

	if (! qlsServer->listen(pipepath)) {
		qWarning() << "SocketRPC: Listen failed";
		delete qlsServer;
		qlsServer = nullptr;
	} else {
		connect(qlsServer, SIGNAL(newConnection()), this, SLOT(newConnection()));
	}
}

void SocketRPC::newConnection() {
	while (true) {
		QLocalSocket *qls = qlsServer->nextPendingConnection();
		if (! qls)
			break;
		new SocketRPCClient(qls, this);
	}
}

bool SocketRPC::send(const QString &basename, const QString &request, const QMap<QString, QVariant> &param) {
	QString pipepath;

#ifdef Q_OS_WIN
	pipepath = basename;
#else
	{
		QString xdgRuntimePath = QProcessEnvironment::systemEnvironment().value(QLatin1String("XDG_RUNTIME_DIR"));
		QDir xdgRuntimeDir = QDir(xdgRuntimePath);

		if (! xdgRuntimePath.isNull() && xdgRuntimeDir.exists()) {
			pipepath = xdgRuntimeDir.absoluteFilePath(basename + QLatin1String("Socket"));
		} else {
			pipepath = QDir::home().absoluteFilePath(QLatin1String(".") + basename + QLatin1String("Socket"));
		}
	}
#endif

	QLocalSocket qls;
	qls.connectToServer(pipepath);
	if (! qls.waitForConnected(1000)) {
		return false;
	}

    // Overthinking much, aren't we?
    /*QDomDocument requestdoc;
	QDomElement req = requestdoc.createElement(request);
	for (QMap<QString, QVariant>::const_iterator iter = param.constBegin(); iter != param.constEnd(); ++iter) {
		QDomElement elem = requestdoc.createElement(iter.key());
		QDomText text = requestdoc.createTextNode(iter.value().toString());
		elem.appendChild(text);
		req.appendChild(elem);
	}
    requestdoc.appendChild(req);

    qls.write(requestdoc.toByteArray());*/
    qls.write(request.toLatin1());
    printf("%s\n",request.toStdString().c_str());
    fflush(stdout);
    qls.flush();
    return true;

	if (! qls.waitForReadyRead(2000)) {
		return false;
	}

	QByteArray qba = qls.readAll();

	QDomDocument replydoc;
	replydoc.setContent(qba);

	QDomElement succ = replydoc.firstChildElement(QLatin1String("reply"));
	succ = succ.firstChildElement(QLatin1String("succeeded"));
	if (succ.isNull())
		return false;

	return QVariant(succ.text()).toBool();
}
