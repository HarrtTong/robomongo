#pragma once

#include <QObject>
#include <QMutex>
#include <unordered_set>

#include <mongo/client/dbclient_rs.h> 

#include "robomongo/core/events/MongoEvents.h"

QT_BEGIN_NAMESPACE
class QThread;
class QTimer;
QT_END_NAMESPACE

namespace Robomongo
{
    class MongoClient;
    class ScriptEngine;
    class ConnectionSettings;

    class MongoWorker : public QObject
    {
        Q_OBJECT

    public:
        enum { pingTimeMs = 60 * 1000 };
        
        using DBClientReplicaSet = std::unique_ptr<mongo::DBClientReplicaSet>;
        using DBClientConnection = std::unique_ptr<mongo::DBClientConnection>;

        explicit MongoWorker(ConnectionSettings *connection, bool isLoadMongoRcJs, int batchSize,
                             int mongoTimeoutSec, int shellTimeoutSec, QObject *parent = nullptr);

        ~MongoWorker();
        void interrupt();
        void stopAndDelete();
        void changeTimeout(int newTimeout);

    protected Q_SLOTS:

        void init();

        /**
         * @brief Every minute we are issuing { ping : 1 } command to every used connection
         * in order to avoid dropped connections.
         */
        void keepAlive();

        /**
         * @brief Initiate connection to MongoDB
         */
        bool handle(EstablishConnectionRequest *event);

        /**
        * @brief Try to re-connect to MongoDB replica set in order to refresh connection view.
        *        For more details, see MongoServer::tryRefreshReplicaSetFolder()
        */
        void handle(RefreshReplicaSetFolderRequest *event);

        /**
         * @brief Load list of all database names
         */
        void handle(LoadDatabaseNamesRequest *event);

        /**
         * @brief Load list of all collection names
         */
        void handle(LoadCollectionNamesRequest *event);

        /**
         * @brief Load list of all users
         */
        void handle(LoadUsersRequest *event);

        /**
        * @brief Load indexes in collection
        */
        void handle(LoadCollectionIndexesRequest *event);

        /**
        * @brief Add/edit indexes in collection
        */
        void handle(AddEditIndexRequest *event);

        /**
        * @brief delete index from collection
        */
        void handle(DropCollectionIndexRequest *event);

        /**
         * @brief Load list of all JS functions
         */
        void handle(LoadFunctionsRequest *event);

        /**
         * @brief Inserts document
         */
        void handle(InsertDocumentRequest *event);

        /**
         * @brief Remove documents
         */
        void handle(RemoveDocumentRequest *event);

        /**
         * @brief Load list of all collection names
         */
        void handle(ExecuteQueryRequest *event);

        /**
         * @brief Execute javascript
         */
        void handle(ExecuteScriptRequest *event);
        void handle(StopScriptRequest *event);

        void handle(AutocompleteRequest *event);
        void handle(CreateDatabaseRequest *event);
        void handle(DropDatabaseRequest *event);

        void handle(CreateCollectionRequest *event);
        void handle(DropCollectionRequest *event);
        void handle(RenameCollectionRequest *event);
        void handle(DuplicateCollectionRequest *event);
        void handle(CopyCollectionToDiffServerRequest *event);

        void handle(CreateUserRequest *event);
        void handle(DropUserRequest *event);

        void handle(CreateFunctionRequest *event);
        void handle(DropFunctionRequest *event);

    protected:
        virtual void timerEvent(QTimerEvent *);

    private:
        // Added after Mongo 4.0 to fix connection failures seen after a first edit/add/remove doc. operation
        void restartReplicaSetConnection();

        /**
         * @brief Send event to this MongoWorker
         */
        void send(Event *event);

        std::vector<std::string> getDatabaseNamesSafe(EstablishConnectionRequest* event = nullptr);
        std::string getAuthBase() const;

        // Returns a pair of DBClientBase* connection and error string
        std::pair<mongo::DBClientBase*, std::string> getConnection(bool mayReturnNull = false);
        MongoClient *getClient();

        /**
        *@brief Reset and update global mongo SSL settings (mongo::sslGlobalParams)
        */
        void configureSSL();

        /**
        *@brief Update global mongo SSL settings (mongo::sslGlobalParams) according to active connection 
        *       request's SSL settings.
        */
        void updateGlobalSSLparams() const;

        /**
        *@brief Reset global mongo SSL settings (mongo::sslGlobalParams) into default zero state
        */
        void resetGlobalSSLparams() const;

        /**
        *@brief Update Replica Set related parameters/settings.
        */
        ReplicaSet getReplicaSetInfo() const;

        std::string connectAndGetReplicaSetName() const;

        /**
         * @brief Send reply event to object 'obj'
         */
        void reply(QObject *receiver, Event *event);

        /**
        * @brief Send hear beat messages to database in order to keep connection alive
        */
        void pingDatabase(mongo::DBClientBase *dbclient) const;

        QThread *_thread;
        QMutex _firstConnectionMutex;

        std::unique_ptr<ScriptEngine> _scriptEngine;

        bool _isAdmin;
        const bool _isLoadMongoRcJs;
        const int _batchSize;
        int _timerId;
        int _dbAutocompleteCacheTimerId;
        int _mongoTimeoutSec;
        int _shellTimeoutSec;
        QAtomicInteger<int> _isQuiting;

        std::unique_ptr<mongo::DBClientConnection> _dbclient;
        std::unique_ptr<mongo::DBClientReplicaSet> _dbclientRepSet;

        ConnectionSettings *_connSettings;

        // Collection of created databases.
        // Starting from 3.0, MongoDB drops empty databases.
        // It means, we did not find a way to create "empty" database.
        // We save all created databases in this collection and merge with
        // list of real databases returned from MongoDB server.
        std::unordered_set<std::string> _createdDbs;
    };

}
