#include "Question.hpp"

#include "emp/math/random_utils.hpp"
#include "functions.hpp"

using emp::MakeCount;

void Question::Print(std::ostream& os) const {
  os << "%- QUESTION " << id << "\n" << question << "\n";
  for (size_t opt_id = 0; opt_id < options.size(); ++opt_id) {
    os << options[opt_id].GetQBLBullet() << " " << options[opt_id].text << '\n';
  }
  os << std::endl;
}

void Question::PrintD2L(std::ostream& os) const {
  os << "NewQuestion,MC,,,\n"
     << "ID,CSE232-" << id << ",,,\n"
     << "Title,,,,\n"
     << "QuestionText," << TextToD2L(question) << ",HTML,,\n"
     << "Points,2,,,\n"
     << "Difficulty,1,,,\n"
     << "Image,,,,\n";
  for (size_t opt_id = 0; opt_id < options.size(); ++opt_id) {
    os << "Option," << (options[opt_id].is_correct ? 100 : 0) << ","
       << TextToD2L(options[opt_id].text) << ",HTML,No feedback.\n";
  }
  os << "Hint,No hint for this one.,,,\n"
     << "Feedback,No feedback,HTML,,\n"
     << ",,,,\n"
     << ",,,,\n";
}

void Question::PrintHTML(std::ostream& os) const { os << std::endl; }

void Question::PrintLatex(std::ostream& os) const { os << std::endl; }

void Question::Validate() {
  // Collect config info for this question.
  correct_range = GetConfig(":correct", emp::Range<size_t>(1, 1));
  option_range =
      GetConfig(":options", emp::Range<size_t>(options.size(), options.size()));

  // Are there enough correct answers?
  const size_t correct_count = CountCorrect();
  emp::notify::TestError(correct_count < correct_range.GetLower(), "Question ",
                         id, " has ",
                         MakeCount(correct_count, "correct answer"), ", but ",
                         correct_range.GetLower(), " required.");
  correct_range.LimitUpper(
      correct_count);  // Cannot have more correct answers than available.

  // Are there too many REQUIRED correct answers?
  const size_t correct_req_count = CountRequiredCorrect();
  emp::notify::TestError(
      correct_req_count > correct_range.GetUpper(), "Question ", id, " has ",
      MakeCount(correct_req_count, "correct answer"),
      "marked required, but at most ", correct_range.GetUpper(), " allowed.");
  correct_range.LimitLower(
      correct_req_count);  // Cannot have few correct answers than required.

  // Are there more REQUIRED options than allowed options?
  const size_t required_count = CountRequired();
  emp::notify::TestError(required_count > option_range.Upper(), "Question ", id,
                         " has ", MakeCount(required_count, "answer"),
                         " marked required, but at most ",
                         option_range.GetUpper(), " options allowed.");
  option_range.LimitLower(
      required_count);  // Must at least select required options.

  // Are there enough available options?
  const size_t incorrect_count = options.size() - correct_count;
  const size_t max_options = incorrect_count + correct_range.Upper();
  emp::notify::TestError(
      max_options < option_range.Lower(), "Question ", id, " has a max of ",
      MakeCount(correct_range.Upper(), "correct answer"), " and ",
      MakeCount(incorrect_count, "other option"), ", but requires at least ",
      option_range.Lower(), "options.");
  option_range.LimitUpper(
      max_options);  // Must at least select required options.

  // Make sure that all fixed-order options are at the beginning or end.
  size_t test_pos = 0;
  while (test_pos < options.size() && options[test_pos].is_fixed)
    test_pos++;  // Front fixed.
  while (test_pos < options.size() && !options[test_pos].is_fixed)
    test_pos++;  // Middle NOT fixed.
  while (test_pos < options.size() && options[test_pos].is_fixed)
    test_pos++;  // Back fixed.
  emp::notify::TestError(test_pos < options.size(), "Question ", id,
                         " has fixed-position options in middle; fixed "
                         "positions must be at start and end.");
}

void Question::ReduceOptions(emp::Random& random, size_t correct_target,
                             size_t incorrect_target) {
  emp_assert(correct_target <= CountCorrect());
  emp_assert(incorrect_target <= CountIncorrect());

  // Pick the set of options to use.
  emp::BitVector used(options.size());
  size_t correct_picks = 0, incorrect_picks = 0;

  // Start with required options.
  for (size_t i = 0; i < options.size(); ++i) {
    if (options[i].is_required) {
      used[i].Set();
      (options[i].is_correct ? correct_picks : incorrect_picks)++;
    }
  }

  // Randomly choose remaining options.
  while (correct_picks < correct_target || incorrect_picks < incorrect_target) {
    size_t pick = random.GetUInt64(options.size());
    if (used[pick]) continue;

    if ((options[pick].is_correct && (correct_picks == correct_target)) ||
        (!options[pick].is_correct && (incorrect_picks == incorrect_target)))
      continue;

    used.Set(pick);
    (options[pick].is_correct ? correct_picks : incorrect_picks)++;
  }

  // Limit to just the answer options that we're using.
  for (size_t i = used.size() - 1; i < used.size(); --i) {
    if (!used[i]) options.erase(options.begin() + i);
  }
}

void Question::ShuffleOptions(emp::Random& random) {
  // Find the option range to shuffle.
  size_t first_id = 0;
  while (first_id < options.size() && options[first_id].is_fixed) first_id++;
  size_t last_id = first_id;
  while (last_id < options.size() && !options[last_id].is_fixed) last_id++;

  emp::ShuffleRange(random, options, first_id, last_id);
}

void Question::Generate(emp::Random& random) {
  // Collect config info for this question (that wasn't collected in Validate(),
  // above) double alt_p = GetConfig(":alt_prob", 0.5);

  size_t correct_target =
      random.GetUInt(correct_range.GetLower(), correct_range.GetUpper() + 1);
  option_range.LimitLower(correct_target);
  size_t option_target =
      random.GetUInt(option_range.GetLower(), option_range.GetUpper() + 1);
  size_t incorrect_target = option_target - correct_target;

  // Trim down the set of options if we need to.
  if (option_target != options.size()) {
    ReduceOptions(random, correct_target, incorrect_target);
  }

  // Reorder the possible answers
  ShuffleOptions(random);
}
