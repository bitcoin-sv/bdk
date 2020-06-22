## Git working flow

We use the fork repository to enfore our git working flow.

- From the main repository, create a fork repository, then work only on that repository (never commit to master branch).
- If the main repository have some change, use bitbucket to sync the fork repository
- Always create a feature branch to work on
- If the fork is out of date, sync the master branch, then rebase the feature branch on the synced master
- Pull request is raised and merged when the job is reviewed and approved

```mermaid
sequenceDiagram
main repo [master]->>forked repo [master]: bitbucket sync
forked repo [master]->>forked repo [feature/my_job]: create [feature/my_job] branch from [master]
loop commits
    forked repo [feature/my_job]->>forked repo [feature/my_job]: job progress
end

main repo [master]-->>forked repo [master]: recync [master] if out of date
forked repo [master]-->>forked repo [feature/my_job]: rebase [feature/my_job] on top of new [master]

loop commits
    forked repo [feature/my_job]->>forked repo [feature/my_job]: continue progressing
end
forked repo [feature/my_job] ->> main repo [master]: Raise PR and merge back to main repo [master]
```

