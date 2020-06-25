## Git working flow

We use the fork repository to enfore our git working flow.

- From the main repository, create a fork repository, then work only on that repository. One rule to remember : `never commit to master branch`.
- If the main repository have some change, use bitbucket to sync the fork repository
- Always create a feature branch to work on
- If the fork is out of date, sync the master branch, then rebase the feature branch on the synced master
- Pull request is raised and merged when after job's been reviewed and approved

```mermaid
graph TD
  subgraph main repo
    C0[master]
    C1[be7568d]
    C2[57740bd]
    C3[HEAD]
    C4[70dd87c]
    C5[2e58424]
    C0-->C1-->C2-->C3-.->|new changes|C4-.->C5
  end
  subgraph fork repo
    F0[master]
    F1[HEAD]
    F2[70dd87c]
    F0-->F1-.->F2
  end
  subgraph fork repo
    FF0[feature/myjob]
    FF1[c165ba3]
    FF2[f1de97f]
    FF3[2e58424]
    FF0-->FF1-->FF2-->FF3
  end

C3-->|sync|F1
F1-->|create|FF0
C4-.->|re-sync|F2
FF0-.->|rebase|F2
FF3-.->|merge pull request|C5

style C0 fill:#3bf52e,stroke:#3bf52e
style C1 fill:#3bf52e,stroke:#3bf52e
style C2 fill:#3bf52e,stroke:#3bf52e
style C3 fill:#3bf52e,stroke:#3bf52e
style C4 fill:#3bf52e,stroke:#3bf52e
style C5 fill:#3bf52e,stroke:#3bf52e
%%style C4 fill:#fec343,stroke:#c40808,stroke-width:4px
style F0 fill:#3bf52e,stroke:#119c07,stroke-width:2px
style F1 fill:#3bf52e,stroke:#119c07,stroke-width:2px
style F2 fill:#fec343,stroke:#c40808,stroke-width:4px
style FF3 fill:#3bf52e,stroke:#119c07,stroke-width:2px
```

