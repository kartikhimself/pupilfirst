module Course = CoursesCurriculum__Course;
module Team = CoursesCurriculum__Team;
module Target = CoursesCurriculum__Target;
module Level = CoursesCurriculum__Level;
module TargetGroup = CoursesCurriculum__TargetGroup;
module LatestSubmission = CoursesCurriculum__LatestSubmission;

/*
 * Create a higher level state abstraction here. Let's pre-calculate the status for
 * all targets, since there are only two (infrequent) actions that can affect this
 * pre-calculation: a student submitting a target, or undoing a previous submission.
 *
 * The higher-level abstraction should pre-calculate and cache intermediary values for
 * the sake of performance - things like target and student's level number, the
 * submission state for each target.
 */

type lockReason =
  | CourseLocked
  | AccessLocked
  | LevelLocked
  | PrerequisitesIncomplete;

type status =
  | Pending
  | Submitted
  | Passed
  | Failed
  | Locked(lockReason);

type t = {
  targetId: string,
  status,
};

type submissionStatus =
  | SubmissionMissing
  | SubmissionPendingReview
  | SubmissionPassed
  | SubmissionFailed;

type cachedTarget = {
  targetId: string,
  targetReviewed: bool,
  levelNumber: int,
  milestone: bool,
  submissionStatus,
  prerequisiteTargetIds: list(string),
};

let isPast = date => date->Belt.Option.mapWithDefault(false, DateFns.isPast);

let makePending = targets =>
  targets |> List.map(t => {targetId: t |> Target.id, status: Pending});

let lockTargets = (targets, reason) =>
  targets
  |> List.map(t => {targetId: t |> Target.id, status: Locked(reason)});

let allTargetsComplete = (targetCache, targetIds) =>
  targetIds
  |> List.for_all(targetId => {
       let cachedTarget =
         targetCache
         |> ListUtils.unsafeFind(
              ct => ct.targetId == targetId,
              "Could not find prerequisite target with ID "
              ++ targetId
              ++ " in list of cached targets",
            );
       cachedTarget.submissionStatus == SubmissionPassed;
     });

let compute =
    (preview, team, course, levels, targetGroups, targets, submissions) =>
  /* Eliminate the two course ended and student access ended conditions. */
  if (preview) {
    makePending(targets);
  } else if (course |> Course.endsAt |> isPast) {
    lockTargets(targets, CourseLocked);
  } else if (team |> Team.accessEndsAt |> isPast) {
    lockTargets(targets, AccessLocked);
  } else {
    /* Cache level number of the student. */
    let studentLevelNumber =
      levels
      |> ListUtils.unsafeFind(
           l => l |> Level.id == Team.levelId(team),
           "Could not student's level with ID " ++ Team.levelId(team),
         )
      |> Level.number;

    /* Cache level number, milestone boolean, and submission status for all targets. */
    let targetsCache =
      targets
      |> List.map(target => {
           let targetId = target |> Target.id;

           let targetGroup =
             targetGroups
             |> ListUtils.unsafeFind(
                  tg => tg |> TargetGroup.id == Target.targetGroupId(target),
                  "Could not find target group with ID "
                  ++ Target.targetGroupId(target)
                  ++ " to create target cache",
                );

           let milestone = targetGroup |> TargetGroup.milestone;

           let levelNumber =
             levels
             |> ListUtils.unsafeFind(
                  l => l |> Level.id == (targetGroup |> TargetGroup.levelId),
                  "Could not find level with ID "
                  ++ Team.levelId(team)
                  ++ " to create target cache",
                )
             |> Level.number;

           let submission =
             submissions
             |> ListUtils.findOpt(s =>
                  s |> LatestSubmission.targetId == targetId
                );

           let submissionStatus =
             switch (submission) {
             | Some(s) =>
               if (s |> LatestSubmission.hasPassed) {
                 SubmissionPassed;
               } else if (s |> LatestSubmission.hasBeenEvaluated) {
                 SubmissionFailed;
               } else {
                 SubmissionPendingReview;
               }
             | None => SubmissionMissing
             };

           {
             targetId,
             targetReviewed: target |> Target.reviewed,
             levelNumber,
             milestone,
             submissionStatus,
             prerequisiteTargetIds: target |> Target.prerequisiteTargetIds,
           };
         });

    /* Scan the targets cache again to form final list of target statuses. */
    targetsCache
    |> List.map(ct => {
         let status =
           switch (ct.submissionStatus) {
           | SubmissionPendingReview => Submitted
           | SubmissionPassed => Passed
           | SubmissionFailed => Failed
           | SubmissionMissing =>
             if (ct.levelNumber > studentLevelNumber && ct.targetReviewed) {
               Locked(LevelLocked);
             } else if (!(
                          ct.prerequisiteTargetIds
                          |> allTargetsComplete(targetsCache)
                        )) {
               Locked(PrerequisitesIncomplete);
             } else {
               Pending;
             }
           };

         {targetId: ct.targetId, status};
       });
  };

let targetId = (t: t) => t.targetId;
let status = t => t.status;

let lockReasonToString = lr =>
  switch (lr) {
  | CourseLocked => "The course has ended and submissions are disabled for all targets!"
  | AccessLocked => "Your access to this course has ended."
  | LevelLocked => "You must level up to complete this target."
  | PrerequisitesIncomplete => "This target has pre-requisites that are incomplete."
  };

let statusToString = t =>
  switch (t.status) {
  | Pending => "Pending"
  | Submitted => "Submitted"
  | Passed => "Passed"
  | Failed => "Failed"
  | Locked(_) => "Locked"
  };

let canSubmit = (~resubmittable, t) =>
  switch (resubmittable, t.status) {
  | (true, Passed)
  | (_, Pending)
  | (_, Failed) => true
  | (false, Passed)
  | (_, Submitted)
  | (_, Locked(_)) => false
  };

let currentLevelStatuses = progressionBehavior =>
  switch (progressionBehavior) {
  | `Limited(_)
  | `Unlimited => [Submitted, Passed]
  | `Strict => [Passed]
  };
let minimumRequiredLevelStatuses = [Passed];

let matchesStatuses = (statuses, ts) => {
  let matchedTargetStatuses =
    ts |> List.filter(t => t.status->List.mem(statuses));

  ts == matchedTargetStatuses;
};
