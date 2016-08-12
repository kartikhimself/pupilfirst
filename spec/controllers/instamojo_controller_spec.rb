require 'rails_helper'

describe InstamojoController do
  let(:application_stage) { create :application_stage, number: 1 }
  let!(:application_stage_2) { create :application_stage, number: 2 }

  let(:batch) do
    create :batch,
      application_stage: application_stage,
      application_stage_deadline: 15.days.from_now,
      next_stage_starts_on: 1.month.from_now,
      batch_stages_attributes: [
        {
          application_stage_id: application_stage.id,
          starts_at: 15.days.ago,
          ends_at: 15.days.from_now
        }
      ]
  end

  let(:batch_application) { create :batch_application, application_stage: application_stage, batch: batch }
  let(:instamojo_payment_request_id) { SecureRandom.hex }
  let(:long_url) { Faker::Internet.url }
  let(:short_url) { Faker::Internet.url }

  before do
    allow_any_instance_of(Instamojo).to receive(:create_payment_request).with(
      amount: batch_application.fee,
      buyer_name: batch_application.team_lead.name,
      email: batch_application.team_lead.email
    ).and_return(
      id: instamojo_payment_request_id,
      status: 'Pending',
      long_url: long_url,
      short_url: short_url
    )
  end

  describe 'GET redirect' do
    let(:payment) { create :payment, batch_application: batch_application }
    let(:payment_id) { SecureRandom.hex }

    before do
      allow_any_instance_of(Instamojo).to receive(:payment_details).with(
        payment_request_id: instamojo_payment_request_id,
        payment_id: payment_id
      ).and_return(
        payment_request_status: 'Completed',
        payment_status: 'Credit',
        fees: '123.45'
      )
    end

    it 'updates payment and associated entries' do
      get :redirect, payment_request_id: payment.instamojo_payment_request_id, payment_id: payment_id

      payment.reload

      expect(payment.instamojo_payment_id).to eq(payment_id)
      expect(payment.instamojo_payment_request_status).to eq('Completed')
      expect(payment.instamojo_payment_status).to eq('Credit')
      expect(payment.fees).to eq(123.45)

      # Expect the application to have moved to stage 2.
      expect(payment.batch_application.application_stage.number).to eq(2)
    end

    it 'redirects to apply page for batch' do
      get :redirect, payment_request_id: payment.instamojo_payment_request_id, payment_id: payment_id
      expect(response).to redirect_to(apply_stage_complete_path(stage_number: '1'))
    end
  end

  describe 'POST webhook' do
    let(:payment) { create :payment, batch_application: batch_application }
    let(:payment_id) { SecureRandom.hex }

    before :all do
      APP_CONFIG[:instamojo][:salt] = 'TEST_SALT'
    end

    after :all do
      APP_CONFIG[:instamojo][:salt] = ENV['INSTAMOJO_SALT']
    end

    it 'updates payment and associated entries' do
      data = "43.21|#{payment_id}|#{payment.instamojo_payment_request_id}|Credit"
      digest = OpenSSL::Digest.new('sha1')
      computed_mac = OpenSSL::HMAC.hexdigest(digest, 'TEST_SALT', data)

      post :webhook,
        payment_request_id: payment.instamojo_payment_request_id,
        payment_id: payment_id,
        status: 'Credit',
        fees: '43.21',
        mac: computed_mac

      payment.reload

      expect(payment.instamojo_payment_id).to eq(payment_id)
      expect(payment.instamojo_payment_request_status).to eq('Completed')
      expect(payment.instamojo_payment_status).to eq('Credit')
      expect(payment.fees).to eq(43.21)

      # Expect the application to have moved to stage 2.
      expect(payment.batch_application.application_stage.number).to eq(2)
    end

    context 'when mac is incorrect or missing' do
      it 'returns 401 Unauthorized' do
        post :webhook,
          payment_request_id: payment.instamojo_payment_request_id,
          payment_id: payment_id,
          status: 'Credit',
          fees: '43.21'

        expect(response.status).to eq(401)
      end
    end
  end
end
