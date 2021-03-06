#ifndef MEDIACONTAINER_H
#define MEDIACONTAINER_H



#include <QString>
#include <QMdiSubWindow>
#include <QAction>


#include "data/cleveruri.h"
#include "preferences/preferencesmanager.h"

/**
 * @brief The MediaContainer class
 */
class MediaContainer : public QMdiSubWindow
{
    Q_OBJECT
public:
    /**
     * @brief MediaContainer
     */
    MediaContainer(QWidget* parent = NULL);
    virtual ~MediaContainer();

    /**
     * @brief setLocalPlayerId
     * @param id
     */
    void setLocalPlayerId(QString id);
    /**
     * @brief getLocalPlayerId
     * @return
     */
    QString getLocalPlayerId();

	/**
	 * @brief setCleverUri
	 * @param uri
	 */
	virtual void setCleverUri(CleverURI* uri);

    virtual CleverURI* getCleverUri() const;

    virtual bool isUriEndWith(QString);

	/**
	 * @brief readFile
	 * @return
	 */
	virtual bool readFileFromUri()=0;


	/**
	 * @brief openFile
	 * @return
	 */
	virtual void openMedia();

	/**
	 * @brief getTitle
	 * @return
	 */
	virtual QString getTitle() const;
    /**
     * @brief setTitle
     * @param str
     */
    virtual void setTitle(QString str);
    /**
     * @brief error
     * @param err - error message to display
     */
    void error(QString err,QWidget* parent);
    void setAction(QAction*);
    QAction* getAction();


signals:
    /**
     * @brief visibleChanged
     */
    void visibleChanged(bool);

public slots:
    /**
     * @brief setVisible
     * @param b
     */
    void setVisible(bool b);

protected:
    QString m_localPlayerId;
	CleverURI* m_uri;
	QString m_title;
    QString m_filter;
    PreferencesManager* m_preferences;
    QAction* m_action;
};

#endif // MEDIACONTAINER_H
