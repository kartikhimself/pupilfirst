let decodeProps = json =>
  Json.Decode.(
    json |> field("currentCourseId", string),
    json |> field("courses", list(StudentCourse__Course.decode)),
    json |> field("additionalLinks", list(string)),
    json |> field("coverImage", optional(string)),
  );

let (currentCourseId, courses, additionalLinks, coverImage) =
  DomUtils.parseJSONAttribute(~id="course-header-root", ()) |> decodeProps;

ReactDOMRe.renderToElementWithId(
  <StudentCourse__Header currentCourseId courses additionalLinks coverImage />,
  "course-header-root",
);
