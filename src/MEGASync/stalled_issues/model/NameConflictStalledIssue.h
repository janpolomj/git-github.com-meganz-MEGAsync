#ifndef NAMECONFLICTSTALLEDISSUE_H
#define NAMECONFLICTSTALLEDISSUE_H

#include <StalledIssue.h>
#include <MegaApplication.h>
#include <FileFolderAttributes.h>
#include <StalledIssuesUtilities.h>

class NameConflictedStalledIssue : public StalledIssue
{
public:
    class ConflictedNameInfo
    {
    public:
        enum class SolvedType
        {
            REMOVE = 0,
            RENAME,
            SOLVED_BY_OTHER_SIDE,
            UNSOLVED
        };

        mega::MegaHandle mHandle;
        QString getUnescapeConflictedName()
        {
            if(mUnescapedConflictedName.isEmpty())
            {
                mUnescapedConflictedName = QString::fromUtf8(MegaSyncApp->getMegaApi()->unescapeFsIncompatible(getConflictedName().toUtf8().constData()));
            }

            return mUnescapedConflictedName;
        }

        QString getConflictedName()
        {
            return mConflictedName;
        }

        QString mConflictedPath;
        QString mRenameTo;
        SolvedType mSolved;
        bool mDuplicated;
        int mDuplicatedGroupId;
        bool mIsFile;
        std::shared_ptr<FileFolderAttributes>  mItemAttributes;

        ConflictedNameInfo()
            : mSolved(SolvedType::UNSOLVED)
        {}

        ConflictedNameInfo(const QFileInfo& fileInfo, bool isFile, std::shared_ptr<FileFolderAttributes> attributes)
            : mConflictedName(fileInfo.fileName()),
              mConflictedPath(fileInfo.filePath()),
              mSolved(SolvedType::UNSOLVED),
              mDuplicatedGroupId(-1),
              mDuplicated(false),
              mIsFile(isFile),
              mItemAttributes(attributes)
        {}

        bool operator==(const ConflictedNameInfo &data)
        {
            return mConflictedName == data.mConflictedName;
        }
        bool isSolved() const {return mSolved != SolvedType::UNSOLVED;}

    private:
        QString mConflictedName;
        QString mUnescapedConflictedName;
    };

    class CloudConflictedNames
    {
    public:
        CloudConflictedNames(QString ufingerprint, int64_t usize, int64_t umodifiedTime)
            : fingerprint(ufingerprint), size(usize), modifiedTime(umodifiedTime)
        {}

        CloudConflictedNames()
        {}

        QString fingerprint;
        int64_t size = -1;
        int64_t modifiedTime = -1;

        bool solved = false;

        QMap<int64_t, std::shared_ptr<ConflictedNameInfo>> conflictedNames;
    };

    class CloudConflictedNamesByHandle
    {
    public:
        CloudConflictedNamesByHandle()
        {}

        void addFolderConflictedName(mega::MegaHandle handle, std::shared_ptr<ConflictedNameInfo> info)
        {
            CloudConflictedNames newConflictedName;
            newConflictedName.conflictedNames.insert(handle, info);
            mConflictedNames.append(newConflictedName);
        }

        void addFileConflictedName(int64_t modifiedtimestamp, int64_t size, int64_t creationtimestamp,
                                   QString fingerprint, std::shared_ptr<ConflictedNameInfo> info)
        {
            for(int index = 0; index < mConflictedNames.size(); ++index)
            {
                auto& namesByHandle = mConflictedNames[index];
                if(fingerprint == namesByHandle.fingerprint
                        && size == namesByHandle.size
                        && modifiedtimestamp == namesByHandle.modifiedTime)
                {
                    auto previousSize = namesByHandle.conflictedNames.size();

                    if(previousSize >= 1)
                    {
                        if(previousSize == 1)
                        {
                            auto firstNameByHandle(namesByHandle.conflictedNames.first());
                            firstNameByHandle->mDuplicatedGroupId = index;
                            firstNameByHandle->mDuplicated = true;
                        }

                        info->mDuplicatedGroupId = index;
                        info->mDuplicated = true;
                    }

                    namesByHandle.conflictedNames.insertMulti(creationtimestamp, info);
                    return;
                }
            }

            CloudConflictedNames newConflictedName(fingerprint, size, modifiedtimestamp);
            newConflictedName.conflictedNames.insertMulti(creationtimestamp,info);
            mConflictedNames.append(newConflictedName);
        }

        bool hasDuplicatedNodes() const
        {
            auto result(false);

            if(!mDuplicatedSolved)
            {
                foreach(auto conflictedNames, mConflictedNames)
                {
                    if(!conflictedNames.solved
                            && conflictedNames.conflictedNames.size() > 1)
                    {
                        result = true;
                        break;
                    }
                }
            }

            return result;
        }

        std::shared_ptr<ConflictedNameInfo> firstNameConflict() const
        {
            foreach(auto& namesByHandle, mConflictedNames)
            {
                if(!namesByHandle.conflictedNames.isEmpty())
                {
                    return namesByHandle.conflictedNames.first();
                }
            }

            return nullptr;
        }

        std::shared_ptr<ConflictedNameInfo> getConflictedNameByIndex(int index) const
        {
            QList<std::shared_ptr<ConflictedNameInfo>> aux = getConflictedNames();
            if(aux.size() > index)
            {
                return aux.at(index);
            }

            return nullptr;
        }

        QList<std::shared_ptr<ConflictedNameInfo>> getConflictedNames() const
        {
            QList<std::shared_ptr<ConflictedNameInfo>> aux;

            foreach(auto& namesByHandle, mConflictedNames)
            {
                if(!namesByHandle.conflictedNames.isEmpty())
                {
                    aux.append(namesByHandle.conflictedNames.values());
                }
            }

            return aux;
        }

        int size() const
        {
            auto counter(0);
            foreach(auto& conflictedName, mConflictedNames)
            {
                counter += conflictedName.conflictedNames.size();
            }

            return counter;
        }

        void clear()
        {
            mConflictedNames.clear();
        }

        bool isEmpty()
        {
            return mConflictedNames.isEmpty();
        }

        void removeDuplicatedNodes()
        {
            std::unique_ptr<StalledIssuesSyncDebrisUtilities> utilities(new StalledIssuesSyncDebrisUtilities());
            QList<mega::MegaHandle> nodesToMove;

            for(int index = 0; index < mConflictedNames.size(); ++index)
            {
                auto& conflictedNamesGroup = mConflictedNames[index];

                if(conflictedNamesGroup.conflictedNames.size() > 1)
                {
                    //The object is auto deleted when finished (as it needs to survive this issue)
                    foreach(auto conflictedName, conflictedNamesGroup.conflictedNames)
                    {
                        if(conflictedName->mSolved == NameConflictedStalledIssue::ConflictedNameInfo::SolvedType::UNSOLVED &&
                           conflictedName != (*(conflictedNamesGroup.conflictedNames.end()-1)))
                        {
                            conflictedName->mSolved = NameConflictedStalledIssue::ConflictedNameInfo::SolvedType::REMOVE;
                            nodesToMove.append(conflictedName->mHandle);
                        }
                    }

                    conflictedNamesGroup.solved = true;
                }
            }

            utilities->moveToSyncDebris(nodesToMove);
            mDuplicatedSolved = true;
        }

    private:
         QList<CloudConflictedNames> mConflictedNames;
         bool mDuplicatedSolved = false;
    };

    NameConflictedStalledIssue(){}
    NameConflictedStalledIssue(const NameConflictedStalledIssue& tdr);
    NameConflictedStalledIssue(const mega::MegaSyncStall *stallIssue);

    void fillIssue(const mega::MegaSyncStall *stall) override;

    const QList<std::shared_ptr<ConflictedNameInfo>>& getNameConflictLocalData() const;
    const CloudConflictedNamesByHandle& getNameConflictCloudData() const;

    bool solveLocalConflictedNameByRemove(int conflictIndex);
    bool solveCloudConflictedNameByRemove(int conflictIndex);

    bool solveCloudConflictedNameByRename(int conflictIndex, const QString& renameTo);
    bool solveLocalConflictedNameByRename(int conflictIndex, const QString& renameTo);

    void renameNodesAutomatically();

    void solveIssue(int option);
    void solveIssue(bool autoSolve) override;

    bool hasDuplicatedNodes() const;

    void updateIssue(const mega::MegaSyncStall *stallIssue) override;

private:
    enum class SideChecked
    {
        Local = 0x01,
        Cloud = 0x02,
        All = Local | Cloud
    };
    Q_DECLARE_FLAGS(SidesChecked, SideChecked);

    bool checkAndSolveConflictedNamesSolved(SidesChecked sidesChecked = SideChecked::All);
    void renameCloudNodesAutomatically(const QList<std::shared_ptr<ConflictedNameInfo>>& cloudConflictedNames,
                                       const QList<std::shared_ptr<ConflictedNameInfo>>& localConflictedNames,
                                       bool ignoreLastModifiedName,
                                       QStringList &cloudItemsBeingRenamed);
    void renameLocalItemsAutomatically(const QList<std::shared_ptr<ConflictedNameInfo>>& cloudConflictedNames,
                                       const QList<std::shared_ptr<ConflictedNameInfo>>& localConflictedNames,
                                       bool ignoreLastModifiedName,
                                       QStringList &cloudItemsBeingRenamed);

    //Rename siblings
    void renameCloudSibling(std::shared_ptr<ConflictedNameInfo> item, const QString& newName);
    void renameLocalSibling(std::shared_ptr<ConflictedNameInfo> item, const QString& newName);

    //Find local or remote sibling
    std::shared_ptr<ConflictedNameInfo> findOtherSideItem(const QList<std::shared_ptr<ConflictedNameInfo>>& items, std::shared_ptr<ConflictedNameInfo> check);

    CloudConflictedNamesByHandle mCloudConflictedNames;
    QList<std::shared_ptr<ConflictedNameInfo>> mLocalConflictedNames;
};

Q_DECLARE_METATYPE(NameConflictedStalledIssue)

#endif // NAMECONFLICTSTALLEDISSUE_H
