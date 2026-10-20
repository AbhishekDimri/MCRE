#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <string>

#include "mcre/core/errors.hpp"
#include "mcre/model/model.hpp"
#include "mcre/model/response.hpp"

using namespace mcre;

namespace {
	// The design document asks for 1e-9 on formulas (reference test T-01).
	constexpr double kTolerance = 1e-9;

	// k = 2, threshold 0.5
	ResponseParams withK(double k) {
		ResponseParams p;
		p.k = k;
		return p;
	}

	ResponseParams withKAndThreshold(double k, double threshold) {
		ResponseParams p;
		p.k = k;
		p.threshold = threshold;
		return p;
	}
}

// ---------- the six curves (reference test T-01: x = 0.5) ----------

TEST(ResponseShape, LinearReturnsTheInputUnchanged) {
	EXPECT_NEAR(responseShape(Mechanism::Linear, 0.5), 0.5, kTolerance);
	EXPECT_NEAR(responseShape(Mechanism::Linear, -0.5), -0.5, kTolerance);  // the sign passes through
	EXPECT_NEAR(responseShape(Mechanism::Linear, 0.0), 0.0, kTolerance);
}

TEST(ResponseShape, InverseShrinksAsTheInputGrows) {
	EXPECT_NEAR(responseShape(Mechanism::Inverse, 0.5), 1.0 / 1.5, kTolerance);  // 0.6667
	EXPECT_NEAR(responseShape(Mechanism::Inverse, 0.0), 1.0, kTolerance);
	EXPECT_LT(responseShape(Mechanism::Inverse, 2.0), responseShape(Mechanism::Inverse, 0.5));
}

TEST(ResponseShape, InverseLooksOnlyAtTheSizeOfTheInput) {
	EXPECT_NEAR(responseShape(Mechanism::Inverse, -0.5), responseShape(Mechanism::Inverse, 0.5), kTolerance);
}

TEST(ResponseShape, SaturationGivesDiminishingReturns) {
	EXPECT_NEAR(responseShape(Mechanism::Saturation, 0.5), 0.5 / 1.5, kTolerance);   // 0.3333
	EXPECT_NEAR(responseShape(Mechanism::Saturation, -0.5), -0.5 / 1.5, kTolerance);  // symmetric
	EXPECT_NEAR(responseShape(Mechanism::Saturation, 0.0), 0.0, kTolerance);

	// With k = 2 the curve climbs toward 1 / k = 0.5 and never reaches it.
	const double far = responseShape(Mechanism::Saturation, 1.0e6, withK(2.0));
	EXPECT_LT(far, 0.5);
	EXPECT_GT(far, 0.499);
}

TEST(ResponseShape, ExponentialIsZeroAtZeroAndGrowsFast) {
	EXPECT_NEAR(responseShape(Mechanism::Exponential, 0.5, withK(2.0)), std::exp(1.0) - 1.0, kTolerance);  // 1.7183
	EXPECT_NEAR(responseShape(Mechanism::Exponential, 0.0), 0.0, kTolerance);
	EXPECT_LT(responseShape(Mechanism::Exponential, -0.5, withK(2.0)), 0.0);  // below zero for a negative input
}

TEST(ResponseShape, ThresholdSwitchesOnAroundTheThreshold) {
	const ResponseParams p = withKAndThreshold(10.0, 0.5);

	EXPECT_NEAR(responseShape(Mechanism::Threshold, 0.5, p), 0.5, kTolerance);  // exactly at the threshold
	EXPECT_NEAR(responseShape(Mechanism::Threshold, 1.0, p), 1.0 / (1.0 + std::exp(-5.0)), kTolerance);  // 0.9933
	EXPECT_NEAR(responseShape(Mechanism::Threshold, 0.0, p), 1.0 / (1.0 + std::exp(5.0)), kTolerance);   // 0.0067
	EXPECT_LT(responseShape(Mechanism::Threshold, 0.0, p), responseShape(Mechanism::Threshold, 0.5, p));
	EXPECT_LT(responseShape(Mechanism::Threshold, 0.5, p), responseShape(Mechanism::Threshold, 1.0, p));
}

TEST(ResponseShape, SigmoidPassesThroughZeroAndIsSymmetric) {
	const ResponseParams p = withK(4.0);

	EXPECT_NEAR(responseShape(Mechanism::Sigmoid, 0.5, p), std::tanh(1.0), kTolerance);  // 0.7616
	EXPECT_NEAR(responseShape(Mechanism::Sigmoid, 0.0, p), 0.0, kTolerance);
	EXPECT_NEAR(responseShape(Mechanism::Sigmoid, -0.5, p), -responseShape(Mechanism::Sigmoid, 0.5, p), kTolerance);
}

TEST(ResponseShape, SigmoidStaysInsideMinusOneAndOne) {
	const ResponseParams p = withK(4.0);

	EXPECT_LT(responseShape(Mechanism::Sigmoid, 3.0, p), 1.0);
	EXPECT_GT(responseShape(Mechanism::Sigmoid, -3.0, p), -1.0);
}

// ---------- the curve only gives a shape ----------

TEST(ResponseShape, DoesNotClampItsResult) {
	// Keeping a value inside its range is the caller's job, so a curve may go above 1.
	EXPECT_GT(responseShape(Mechanism::Exponential, 2.0, withK(2.0)), 1.0);
	EXPECT_GT(responseShape(Mechanism::Linear, 5.0), 1.0);
}

TEST(ResponseShape, GivesTheSameAnswerEveryTime) {
	const double first = responseShape(Mechanism::Sigmoid, 0.37, withK(4.0));
	EXPECT_EQ(responseShape(Mechanism::Sigmoid, 0.37, withK(4.0)), first);  // exact, not "near"
}

// ---------- unfinished and invalid ----------

TEST(ResponseShape, CustomIsNotImplementedYet) {
	EXPECT_THROW(responseShape(Mechanism::Custom, 0.5), NotImplementedError);

	try {
		responseShape(Mechanism::Custom, 0.5);
		FAIL() << "expected NotImplementedError";
	}
	catch (const NotImplementedError& e) {
		EXPECT_NE(std::string(e.what()).find("custom mechanism"), std::string::npos);
	}
}

TEST(ResponseShape, AnUnknownMechanismThrows) {
	EXPECT_THROW(responseShape(static_cast<Mechanism>(99), 0.5), std::invalid_argument);
}
