/*************************************************************************
 *   Copyright (C) 2007 by Romain Campioni                               *
 *   Copyright (C) 2009 by Renaud Guezennec                              *
 *   Copyright (C) 2011 by Joseph Boudou                                 *
 *                                                                       *
 *   http://www.rolisteam.org/                                           *
 *                                                                       *
 *   Rolisteam is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published   *
 *   by the Free Software Foundation; either version 2 of the License,   *
 *   or (at your option) any later version.                              *
 *                                                                       *
 *   This program is distributed in the hope that it will be useful,     *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of      *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the       *
 *   GNU General Public License for more details.                        *
 *                                                                       *
 *   You should have received a copy of the GNU General Public License   *
 *   along with this program; if not, write to the                       *
 *   Free Software Foundation, Inc.,                                     *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.           *
 *************************************************************************/


#include "chatwindow.h"

#include <QDateTime>
#include <QScrollBar>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QDir>
#include <QPushButton>

#include "chat/chat.h"
#include "network/networkmessagewriter.h"
#include "initialisation.h"
#include "localpersonmodel.h"
#include "mainwindow.h"
#include "data/persons.h"
#include "userlist/playersList.h"
#include "chat/improvedtextedit.h"

#include "types.h"

QStringList ChatWindow::m_keyWordList;

ChatWindow::ChatWindow(AbstractChat * chat, MainWindow * parent)
    : QWidget(), m_chat(chat), m_filename("%1/%2.html"),m_mainWindow(parent)/*,ui(new Ui::ChatWindow)*/
{
    m_preferences = PreferencesManager::getInstance();
    //ui->setupUi(this);

    if (m_chat == NULL)
    {
        qFatal("ChatWindow with NULL chat");
    }

    // Initialisation des variables
    m_warnedEmoteUnavailable = false;
    m_hasUnseenMessage = false;

    // static members
    if (m_keyWordList.size() == 0)
    {
        m_keyWordList << "e " << "em " << "me " << "emote ";
    }
    setupUi();

    connect(m_editionZone, SIGNAL(textValidated(QString,QString)), this, SLOT(emettreTexte(QString,QString)));

    connect(m_editionZone, SIGNAL(ctrlUp()), this, SLOT(upSelectPerson()));
    connect(m_editionZone, SIGNAL(ctrlDown()), this, SLOT(downSelectPerson()));

    connect(m_mainWindow, SIGNAL(closing()), this, SLOT(save()));
     connect(m_chat, SIGNAL(changedMembers()), this, SLOT(scheduleUpdateChatMembers()));

    m_toggleViewAction = new QAction(this);
    m_toggleViewAction->setCheckable(true);
}
QMdiSubWindow* ChatWindow::getSubWindow()
{
    return m_window;
}

void ChatWindow::setupUi()
{

    // create and connect toggleViewAction




    m_splitter = new QSplitter();
    m_splitter->setOrientation(Qt::Vertical);





    //ChatRoom
    QVBoxLayout * vboxLayout = new QVBoxLayout();
    vboxLayout->setMargin(0);
    vboxLayout->setSpacing(0);


    vboxLayout->addWidget(m_splitter);
    m_displayZone= new ChatBrowser();
    m_displayZone->setOpenExternalLinks(true);
    m_displayZone->setReadOnly(true);
    m_displayZone->setMinimumHeight(30);



    m_editionZone = new ImprovedTextEdit();
    m_editionZone->setReadOnly(false);
    m_editionZone->setMinimumHeight(30);
    m_editionZone->setAcceptRichText(false);
    m_editionZone->installEventFilter(this);

    connect(m_editionZone,SIGNAL(receivedFocus()),this,SLOT(editionGetFocus()));












    // Layout
    m_bottomWidget = new QWidget();

    m_selectPersonComboBox = new QComboBox();
    m_selectPersonComboBox->setModel(&LocalPersonModel::instance());
    m_selectPersonComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
//    if()
//    m_selectPersonComboBox->setCurrentIndex(0);

    // Toolbar
    QToolBar * toolBar = new QToolBar();
    //toolBar->setOrientation(Qt::Horizontal);
    toolBar->addWidget(m_selectPersonComboBox);
    toolBar->addAction(QIcon::fromTheme("document-save", QIcon(":/resources/icons/save.png")),tr("save"), this, SLOT(save()));

    // SelectPersonComboBox



    scheduleUpdateChatMembers();




    //m_save = new QPushButton(QIcon::fromTheme("document-save", QIcon(":/resources/icons/save.png")),tr("save"),this);



    //Typing Zone










    QVBoxLayout * internalVLayout = new QVBoxLayout();
    internalVLayout->setMargin(0);
    internalVLayout->setSpacing(0);

    internalVLayout->addWidget(toolBar);
    internalVLayout->addWidget(m_editionZone);
//zoneEdition
    m_bottomWidget->setLayout(internalVLayout);



    m_splitter->addWidget(m_displayZone);
    m_splitter->addWidget(m_bottomWidget);


    // Initialisation des tailles des 2 widgets
    QList<int> tailles;
    tailles.append(200);
    tailles.append(40);
    m_splitter->setSizes(tailles);

    // Connexion du signal de validation du text de la zone d'edition a la demande d'emission

    // Window initialisation
    setObjectName("ChatWindow");






    setAttribute(Qt::WA_DeleteOnClose, false);

    setLayout(vboxLayout);

}

/********************************************************************/    
/* Destructeur                                                      */
/********************************************************************/    
ChatWindow::~ChatWindow()
{
    delete m_toggleViewAction;

    if (PreferencesManager::getInstance()->value("isClient",true).toBool() || !m_chat->inherits("PrivateChat"))
        delete m_chat;
}


AbstractChat * ChatWindow::chat() const
{
    return m_chat;
}
bool ChatWindow::isVisible()
{
    return (m_window->isVisible() & QWidget::isVisible());
}

// not (const QString & message), because we change it !
void ChatWindow::emettreTexte(QString messagehtml,QString message)
{
    //NetMsg::ChatMessageAction, NetMsg::DiceMessageAction, NetMsg::EmoteMessageAction
    NetMsg::Action action = NetMsg::DiceMessageAction;

    bool ok=true;
    QString tirage;
    int result;
    m_editionZone->clear();


    QString localPersonIdentifier = m_selectPersonComboBox->itemData(m_selectPersonComboBox->currentIndex(), PlayersList::IdentifierRole).toString();
    Person * localPerson = PlayersList::instance()->getPerson(localPersonIdentifier);

    QString tmpmessage=message.simplified();
    QString messageCorps="";
    QString messageTitle="";
    QColor color;

    switch(tmpmessage[0].toLatin1())
    {
    case '!':
        result = calculerJetDes(message, tirage, ok);
        if(ok)
        {
            messageCorps = tr("got %1 at your dice roll [%2]").arg(result).arg(tirage);
            messageTitle = tr("You");
            color = localPerson->color();
            showMessage(messageTitle, color, messageCorps,NetMsg::DiceMessageAction);
            message = QString(tr("got %1 at his dice roll [%2]").arg(result).arg(tirage));
        }
        else
        {
            messageCorps = tr("!1d6 or !5d10+3 or !2d20-3d10+1d6+5 etc... The dice roll is public (For private roll, use & ).");
            messageTitle = tr("Syntax");
            color = Qt::red;
            showMessage(messageTitle, color, messageCorps);
        }
        break;
    case '&':
        result = calculerJetDes(message, tirage, ok);
        if (ok)
        {
            messageCorps = tr("You got %1 at your secret dice roll [%2]").arg(result).arg(tirage);
            messageTitle = tr("Secret Roll:");
            color = Qt::magenta;
            showMessage(messageTitle, color,messageCorps ,NetMsg::DiceMessageAction);
        }
        else
        {
            messageCorps = tr("!1d6 or !5d10+3 or !2d20-3d10+1d6+5 etc... The dice roll is public (For private roll, use & ).");
            messageTitle = tr("Syntax");
            color = Qt::red;
            showMessage(messageTitle, color, messageCorps);
        }
        return;
        break;
    case '*':
    {
        QRegExp exp("\\*[0-9]+[dD].*");
        if(exp.exactMatch(message))
        {
            QString glitch;
            result = calculerJetDesSR4(message, tirage, glitch, ok);
            if (ok)
            {
                messageCorps = tr("got %1 successes %2%3").arg(result).arg(glitch).arg(tirage);
                messageTitle = tr("You");
                // On affiche le resultat du tirage dans la zone d'affichage
                showMessage(messageTitle, localPerson->color(),messageCorps ,NetMsg::DiceMessageAction);
                // On cree un nouveau message a envoyer aux autres utilisateurs
                message = QString(tr("got %1 successes %2%3").arg(result).arg(glitch).arg(tirage));
            }
            else
            {
                //*12D ... ajoutez R pour rusher, G3 pour les Gremlins d'indice 3 et + pour relancer les 6 ... ajouter C pour ne pas afficher les détails du lancer, et S pour n'afficher que les résultats.
                messageCorps = tr("*12D ... Add R for rusher, G3 for Gremlins index of 3 and + for reroll 6 ... Adde C hide roll detail, and S to display the result only.");
                messageTitle = tr("Syntax SR4");
                color = Qt::red;
                showMessage(messageTitle, color, messageCorps);
            }
            break;
        }
        else
        {
            messageTitle = tr("You");
            /*if(m_chat->everyPlayerHasFeature(QString("RichTextChat")))
                        {
                            message=messagehtml;
                        }*/
            showMessage(messageTitle, localPerson->color(), message);

            // action is messageChatWindow only if there are no dices
            action = NetMsg::ChatMessageAction;
            break;
        }
    }

    case '/':
    {
        bool emote = false;

        for(int i = 0;((i < m_keyWordList.size())&&(!emote)); i++)
        {
            QString s = m_keyWordList[i];

            if(tmpmessage.indexOf(s) == 1)
            {
                emote = true;

                message.remove(0,s.length()+1);
            }

        }
        if(emote)
        {
            // Warn if some users don't have Emote(0) feature
            if (!m_warnedEmoteUnavailable && !m_chat->everyPlayerHasFeature(QString("Emote")))
            {
                messageTitle = tr("Warning");
                messageCorps = tr("Some users won't be enable to see your emotes.");
                color = Qt::red;
                showMessage(messageTitle, color, messageCorps);
                m_warnedEmoteUnavailable = true;
            }


            showMessage(localPerson->name(), localPerson->color(), message,NetMsg::EmoteMessageAction);
            action = NetMsg::EmoteMessageAction;
            break;
        }
    }

    default:
        messageTitle = localPerson->name();
        /* if(m_chat->everyPlayerHasFeature(QString("RichTextChat")))
            {
                message=messagehtml;
            }*/
        showMessage(messageTitle, localPerson->color(), message);
        // action is messageChatWindow only if there are no dices
        action = NetMsg::ChatMessageAction;
        break;
    }

    if(!ok)
        return;

    // Emission du message
    NetworkMessageWriter data(NetMsg::ChatCategory, action);
    data.string8(localPersonIdentifier);
    data.string8(m_chat->identifier());
    data.string32(message);
    m_chat->sendThem(data);
}


QAction * ChatWindow::toggleViewAction() const
{
    return m_toggleViewAction;
}


void ChatWindow::showMessage(const QString& utilisateur, const QColor& couleur, const QString& message, NetMsg::Action msgtype)
{
    // On repositionne le curseur a la fin du QTexEdit
    m_displayZone->moveCursor(QTextCursor::End);
    // Affichage du nom de l'utilisateur
    m_displayZone->setTextColor(couleur);
    QString msg = message;
    switch(msgtype)
    {
    case NetMsg::ChatMessageAction:
        m_displayZone->setFontItalic(false);
        m_displayZone->append(QString("%1 : ").arg(utilisateur));
        break;
    case NetMsg::EmoteMessageAction:
        m_displayZone->setFontItalic(true);
        m_displayZone->append(QString("%1 ").arg(utilisateur));
        msg=QString("<i>%1</i>").arg(message);
        break;
    case NetMsg::DiceMessageAction:
        m_displayZone->setFontItalic(false);
        m_displayZone->append(QString("%1 ").arg(utilisateur));
        break;
    default:
        qWarning("Can't type unknown message type %d", msgtype);
        return;
    }

    if (!m_editionZone->hasFocus() && !m_hasUnseenMessage)
    {
        m_hasUnseenMessage = true;
        emit ChatWindowHasChanged(this);
    }

    // Affichage du message
    m_displayZone->setTextColor(Qt::black);


    m_displayZone->insertHtml(msg);
   // qDebug() << message;
    if(msgtype==NetMsg::EmoteMessageAction)
    {
        m_displayZone->setFontItalic(false);
    }
    // On repositionne la barre de defilement, pour pouvoir voir le texte qui vient d'etre affiche
    m_displayZone->verticalScrollBar()->setSliderPosition(m_displayZone->verticalScrollBar()->maximum());
}

int ChatWindow::calculerJetDes(QString & message, QString & tirage, bool &ok)
{
    //1 by default to add, -1 to substract.
    short signOperator=1;
    int result = 0;
    int nombre=0; ///  @attention : Initialize value Otherwise something wrong could happen
    int faces=0;
    int garde=0;
    int seuil=0;
    bool relance10;

    // Mise a 0 de la chaine tirage
    tirage.clear();
    // Suppression des espaces dans le message
    message.remove(QChar(' '));
    // Suppresion du "!" en tete du message
    message.remove(0, 1);
    // Si la taille est nulle, on quitte avec une erreur
    if (!message.size())
        return (ok = false);

    while(message.size())
    {

        if(GetNumber(message,&nombre))///  @attention : former fashion did not take care of the return value.
        {
            // 4 cas de figure :
            // le nombre est suivi par un "D" : on recupere le nombre qui suit (nbr de faces du de)
            if (message[0] == QChar('d') || message[0] == QChar('D'))
            {
                // On supprime le "D"
                message.remove(0, 1);
                // Si la taille est nulle, on quitte avec une erreur
                if (!message.size())
                    return (ok = false);
                ok = GetNumber(message,&faces);
                // S'il y a un nombre de des ou de faces nul, on quitte
                if (!nombre || !faces)
                    return (ok = false);
                // The dices rolling
                QList<int> listDices;
                unsigned short dice, sumDice=0;
                for(unsigned short u=0;u<nombre;u++)
                {
                    dice=(qrand()%faces)+1;
                    listDices.append(dice);
                    sumDice+=dice;
                }
                // Formatting the "tirage" text
                tirage.append(tr("%1d%2 (%3").arg(nombre).arg(faces).arg(listDices[0]));
                for(unsigned short u=1;u<listDices.size();u++)
                {
                    tirage.append(QString(","));
                    tirage.append(QString::number(listDices[u]));
                }
                tirage.append(QString(")"));
                result+=signOperator*sumDice;
            } // Fin de la lecture et du tirage des des
            else if (message[0] == QChar('g') || message[0] == QChar('G'))
            {
                relance10=(message[0] == QChar('G'));
                // On supprime le "G"
                message.remove(0, 1);
                // Si la taille est nulle, on quitte avec une erreur
                if (!message.size())
                    return (ok = false);
                ok = GetNumber(message,&garde);
                // S'il y a un nombre de des ou de faces nul, on quitte
                if (!nombre || !garde)
                    return (ok = false);
                if(garde > nombre)
                    garde = nombre;
                // The dices rolling
                QList<int> listDices;
                unsigned short tmpDice, dice, sumDice=0;
                for(unsigned short u=0;u<nombre;u++)
                {
                    tmpDice=(qrand()%10)+1;
                    dice=tmpDice;
                    if(relance10)
                        while((tmpDice==10))
                        {
                            tmpDice=(qrand()%10)+1;
                            dice+=tmpDice;
                        }
                    listDices.append(dice);
                    //sumDice+=dice;
                }
                qSort(listDices.begin(), listDices.end(), qGreater<unsigned short>());
                for(int it=0;it<garde;++it)
                {
                    sumDice+=listDices[it];
                }
                // Formatting the "tirage" text
                tirage.append(tr("%1g%2 (%3").arg(nombre).arg(garde).arg(listDices[0]));
                for(unsigned short u=1;u<listDices.size();u++)
                {
                    tirage.append(QString(","));
                    tirage.append(QString::number(listDices[u]));
                }
                tirage.append(QString(")"));
                result+=signOperator*sumDice;
            } // Fin de la lecture et du tirage des des
            // le nombre est suivi par un "s" : on recupere pas le nombre qui suit (il n'y en a pas)
            else if (message[0] == QChar('s') || message[0] == QChar('S'))
            {
                // On supprime le "S"
                message.remove(0, 1);
                // S'il y a un nombre de des ou de faces nul, on quitte
                if (!nombre)
                    return (ok = false);
                // The dices rolling
                QList<int> listDices;
                unsigned short dice, sumDice=0;
                for(unsigned short u=0;u<nombre;u++)
                {
                    dice=(qrand()%6)+1;
                    listDices.append(dice);
                    if(dice>=5)
                        ++sumDice ;
                }
                // Formatting the "tirage" text
                tirage.append(tr("%1s (%3").arg(nombre).arg(listDices[0]));
                for(unsigned short u=1;u<listDices.size();u++)
                {
                    tirage.append(QString(","));
                    tirage.append(QString::number(listDices[u]));
                }
                tirage.append(QString(")"));
                result+=signOperator*sumDice;
            } // Fin de la lecture et du tirage des des
            // le nombre est suivi par un "E" : on recupere le nombre qui suit (valeur de carac)
            else if (message[0] == QChar('e') || message[0] == QChar('E'))
            {
                relance10=(message[0] == QChar('E'));
                // On supprime le "E"
                message.remove(0, 1);
                // Si la taille est nulle, on quitte avec une erreur
                if (!message.size())
                    return (ok = false);
                ok = GetNumber(message,&seuil);
                // S'il y a un nombre de des ou de faces nul, on quitte
                if (!nombre || !seuil)
                    return (ok = false);
                // The dices rolling
                QList<int> listDices;
                unsigned short tmpDice, dice, sumDice=0, success=0;
                for(unsigned short u=0;u<nombre;u++)
                {
                    tmpDice=(qrand()%10)+1;
                    dice=tmpDice;
                    if(relance10)
                        while((tmpDice==10))
                        {
                            tmpDice=(qrand()%10)+1;
                            dice+=tmpDice;
                        }
                    listDices.append(dice);
                }
                qSort(listDices.begin(), listDices.end(), qGreater<unsigned short>());
                int it =0;
                while (sumDice<seuil)
                {
                    sumDice+=listDices[it];
                    success++;
                    it++;
                }
                // Formatting the "tirage" text
                tirage.append(tr("%1e%2 (%3").arg(nombre).arg(seuil).arg(listDices[0]));
                for(unsigned short u=1;u<listDices.size();u++)
                {
                    tirage.append(QString(","));
                    tirage.append(QString::number(listDices[u]));
                }
                tirage.append(QString(")"));
                result+=signOperator*success;
            } // Fin de la lecture et du tirage des des

            // le nombre est suivi par un "V" : on recupere le nombre qui suit (valeur de carac)
            else if (message[0] == QChar('v') || message[0] == QChar('V'))
            {
                relance10=(message[0] == QChar('V'));
                // On supprime le "V"
                message.remove(0, 1);
                // Si la taille est nulle, on quitte avec une erreur
                if (!message.size())
                    return (ok = false);
                ok = GetNumber(message,&seuil);
                // S'il y a un nombre de des ou de faces nul, on quitte
                if (!nombre || !seuil)
                    return (ok = false);
                // The dices rolling
                QList<int> listDices;
                unsigned short dice, success=0;
                for(unsigned short u=0;u<nombre;u++)
                {
                    dice=(qrand()%10)+1;
                    if(relance10 && dice == 10)
                        nombre++;                     							    listDices.append(dice);
                }
                for (int it=0; it<nombre; it++)
                {
                    if (listDices[it]>=seuil)
                        success++;
                }

                // Formatting the "tirage" text
                tirage.append(tr("%1v%2 (%3").arg(nombre).arg(seuil).arg(listDices[0]));
                for(unsigned short u=1;u<listDices.size();u++)
                {
                    tirage.append(QString(","));
                    tirage.append(QString::number(listDices[u]));
                }
                tirage.append(QString(")"));
                result+=signOperator*success;
            } // Fin de la lecture et du tirage des des
            // le nombre est suivi par un "W" : on recupere le nombre qui suit (valeur de carac)
            else if (message[0] == QChar('w') || message[0] == QChar('W'))
            {
                relance10=(message[0] == QChar('W'));
                // On supprime le "W"
                message.remove(0, 1);
                // Si la taille est nulle, on quitte avec une erreur
                if (!message.size())
                    return (ok = false);
                ok = GetNumber(message,&seuil);
                // S'il y a un nombre de des ou de faces nul, on quitte
                if (!nombre || !seuil)
                    return (ok = false);
                // The dices rolling
                QList<int> listDices;
                unsigned short dice;
                int success=0;
                bool  critique=true;
                for(unsigned short u=0;u<nombre;u++)
                {
                    dice=(qrand()%10)+1;
                    if(relance10 && dice == 10)
                        nombre++;                     							    listDices.append(dice);
                }
                for (int it=0; it<nombre; it++)
                {
                    if (listDices[it]>=seuil)
                    {
                        success++;
                        critique=false;

                    }
                    if (listDices[it]==1)
                        success--;
                }
                if (success<0 && !critique)
                    success=0;

                // Formatting the "tirage" text
                tirage.append(tr("%1w%2 (%3").arg(nombre).arg(seuil).arg(listDices[0]));
                for(unsigned short u=1;u<listDices.size();u++)
                {
                    tirage.append(QString(","));
                    tirage.append(QString::number(listDices[u]));
                }
                tirage.append(QString(")"));
                result+=signOperator*success;
            } // Fin de la lecture et du tirage des des

            // Le caractere suivant n'est pas un "D" : on ajoute ou on soustrait le nombre
            else
            {
                result+=signOperator*nombre;
                tirage+=QString("%1").arg(nombre);
            }
            // On regarde si le caractere suivant est un signe + ou -
            if (message.size())
            {
                if (message[0] == QChar('+'))
                    signOperator = 1;
                else if (message[0] == QChar('-'))
                    signOperator = -1;
                else
                    return (ok = false);/// @attention
                tirage+=' ';
                tirage+=message[0];
                tirage+=' ';
                // On supprime l'operateur
                message.remove(0, 1);
                // Le message ne doit pas etre vide apres
                if (message.size() == 0)
                    return (ok = false);
            }
        }
        else
        {
            ok=false;
            return 0;
        }
    }

    ok = true;
    return result;
}

// Ultyme
/********************************************************************/    
/* Analyse le message et calcule le jet de des selon les regles de  */
/* SR4 si la syntaxe est correcte (ok == true). Une Qstring est ge- */
/* neree pour expliquer le tirage, un autre pour le glitch.         */
/* Les Gremlins et la regle du 6 again (usage de l'edge) sont geres.*/
/********************************************************************/    
int ChatWindow::calculerJetDesSR4(QString &message, QString &tirage, QString &glitch, bool &ok)
{
    //Initialisation du nombre de succes, de glitches, et des parametres du lancer
    int nbDes;
    int nbDesEnCours;
    int nbDesEnsuite;
    QString nbDesStr;
    int nbSucces = 0;
    int nbGlitch = 0;
    bool sixAgainActif = false;
    bool gremlinsActif = false;
    bool rushActif = false;
    bool modeCourtActif = false;
    bool modeSecretActif = false;
    int indiceGremlins = 0;
    QString indiceGremlinsStr;
    int nbFaces = 6;
    QString nbFacesStr = "6";
    int seuilSucces = 5;
    int seuilGlitch = 1;
    int nbPasses = 0;
    int de;
    bool estOK;

    tirage.clear();
    glitch.clear();

    message.remove(QChar(' '));
    message.remove(0, 1);

    if (message.size() == 0)
        return (ok=false);

    ok=GetNumber(message,&nbDes);

    if (message.size() == 0 || (message[0] != QChar('d') && message[0] != QChar('D')))
        return (ok=false);
    message.remove(0, 1);

    while (message.size()>0)
    {
        QChar firstChar=message[0];
        //if the first character of message is a letter, put it to lowercase
        if(firstChar.isLetter())
            firstChar.toCaseFolded ();

        switch(firstChar.toLatin1())
        {
        case '+':
            if (sixAgainActif)
                return (ok=false);

            sixAgainActif = true;
            break;
        case 'g':
            if (gremlinsActif)
                return (ok=false);

            gremlinsActif = true;
            message.remove(0, 1);

            // the function need to have a message and that message[0] is a digit to continue
            if (!(message.size() && message[0].isDigit()))
                return (ok=false);
            indiceGremlinsStr = message.left(1);
            indiceGremlins = indiceGremlinsStr.toUInt(&estOK, 10);
            break;
        case 'r':
            if (rushActif)
                return (ok=false);

            rushActif = true;
            seuilGlitch = 2;
            break;
        case 'c':
            if (modeCourtActif)
                return (ok=false);

            modeCourtActif = true;
            break;
        case 's':
            if (modeSecretActif)
                return (ok=false);

            modeSecretActif = true;
            break;
        default:
            return (ok=false);
        }
        message.remove(0, 1);
    }



    if (nbDes == 0)
        return (ok=false);

    if (!modeSecretActif)
    {
        tirage.append(QString(tr(" (%1 dices").arg(nbDes)));
        if (rushActif)
            tirage.append(QString(tr(" in rushing")));
        if (gremlinsActif)
            tirage.append(QString(tr(" with Gremlins %1").arg(indiceGremlinsStr)));
        if (sixAgainActif)
            tirage.append(QString(tr(" - 6 are reroll")));
        if (!modeCourtActif)
            tirage.append(QString(tr(" : ")));
        else
            tirage.append(QString(tr(")")));
    }

    nbDesEnsuite = nbDes;
    while (nbDesEnsuite != 0)
    {
        nbDesEnCours = nbDesEnsuite;
        nbDesEnsuite = 0;
        nbPasses++;

        for (int u=0; u<nbDesEnCours ; u++)
        {
            // Tirage du de
            de = 1 + (int)((double)rand() / ((double)RAND_MAX + 1) * nbFaces); //rand()%nbFaces + 1;
            // Verification du succes
            if (de >= seuilSucces)
            {
                nbSucces++;
                //Si la regle des 6 again est activee, augmentation de la prochaine serie
                if (sixAgainActif && de == 6)
                    nbDesEnsuite++;
            }
            // Verification du glitch (uniquement sur la premiere serie de lancer)
            // this is strange... nbPasses '=' 1 ??? not '==' ?
            if ((nbPasses = 1) && (de <= seuilGlitch))
                nbGlitch++;
            // Ajout du resultat a la chaine Tirage si pas en Mode court ou Mode Secret
            if (!modeCourtActif && !modeSecretActif)
            {
                tirage.append(QString::number(de));
                // Ajout d'un espace, d'un "puis" entre deux series ou d'une parenthese pour finir
                if (u < nbDesEnCours-1)
                    tirage.append(QString(" "));
                else if (nbDesEnsuite > 0)
                    tirage.append(QString(" puis "));
                else
                    tirage.append(QString(")"));
            }
        }
    }

    if (nbGlitch>0 && nbGlitch>((nbDes/2)-indiceGremlins))
    {
        if (nbSucces == 0)
            glitch.append(QString(tr(" and one critical complication ")));
        else
            glitch.append(QString(" and one complication "));
    }


    ok = true;
    return nbSucces;
}
// FIN Ultyme

bool ChatWindow::hasUnseenMessage() const
{
    return m_hasUnseenMessage;
}



void ChatWindow::save()
{
    QString filename = m_filename.arg(m_preferences->value("ChatDirectory",QDir::homePath()).toString(), m_chat->name());
    QFile file (filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qWarning("Unable to open %s", qPrintable(filename));
        return;
    }

    QTextStream stream (&file);
    stream << m_displayZone->document()->toHtml(QByteArray("utf-8"));

    file.close();
}

QString ChatWindow::getTitleFromChat()
{
    m_toggleViewAction->setText(m_chat->name());
    return m_chat->name();
}


void ChatWindow::showEvent(QShowEvent *event)
{
    m_editionZone->setFocus(Qt::OtherFocusReason);
    if(m_selectPersonComboBox->currentText().isEmpty())
    {
        m_selectPersonComboBox->setCurrentIndex(0);
    }
    if(NULL!=m_toggleViewAction)
    {
        m_toggleViewAction->setChecked(true);
    }
    emit ChatWindowHasChanged(this);
    //QWidget::showEvent(event);
}
void ChatWindow::hideEvent(QHideEvent *event)
{
    if(NULL!=m_toggleViewAction)
    {
        m_toggleViewAction->setChecked(false);
    }
    emit ChatWindowHasChanged(this);
    //QWidget::hideEvent(event);
}

void ChatWindow::editionGetFocus()
{
    m_hasUnseenMessage = false;
    emit ChatWindowHasChanged(this);
}


bool ChatWindow::GetNumber(QString &str, int* value)
{
    bool error;
    int i=0;
    while(str[i].isDigit() && i<str.size())
        i++;
    // Length of 0 (No digit): exit with error
    if(!i)
        return false;

    *value = str.left(i).toUInt(&error, 10);
    str.remove(0, i);
    return error;
}

void ChatWindow::upSelectPerson()
{
    int pos = m_selectPersonComboBox->currentIndex();
    if (pos == 0)
        pos = m_selectPersonComboBox->count();
    m_selectPersonComboBox->setCurrentIndex(pos - 1);
}

void ChatWindow::downSelectPerson()
{
    int pos = m_selectPersonComboBox->currentIndex() + 1;
    if (pos == m_selectPersonComboBox->count())
        pos = 0;
    m_selectPersonComboBox->setCurrentIndex(pos);
}

void ChatWindow::scheduleUpdateChatMembers()
{
    //qDebug("schedules");
    QTimer::singleShot(300, this, SLOT(updateChatMembers()));
}

void ChatWindow::updateChatMembers()
{
    //qDebug("doing it");
    bool enable = m_chat->everyPlayerHasFeature(QString("MultiChat"));
    if (!enable)
    {
        m_selectPersonComboBox->setCurrentIndex(0);
    }
    m_selectPersonComboBox->setEnabled(enable);
}
void ChatWindow::setSubWindow(QMdiSubWindow* subWindow)
{
    m_window = subWindow;
    connect(m_toggleViewAction, SIGNAL(triggered(bool)), m_window, SLOT(setVisible(bool)));
}

