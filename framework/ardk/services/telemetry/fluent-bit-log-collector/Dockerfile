FROM debian:bullseye

RUN apt-get update && apt-get install -y \
    openssl \
    curl \
    gnupg

RUN curl -fsSL --retry 3 https://packages.fluentbit.io/fluentbit.key | gpg --dearmor > /usr/share/keyrings/fluentbit-keyring.gpg
RUN echo "deb [signed-by=/usr/share/keyrings/fluentbit-keyring.gpg] https://packages.fluentbit.io/debian/bullseye bullseye main" | tee /etc/apt/sources.list.d/fluent.list
RUN apt-get update && apt-get install -y fluent-bit

# Copy the config and launch script in last, so config edits don't re-run the
# network fetches above (LF line endings are guaranteed by .gitattributes)
COPY ./config.conf /etc/fluent-bit/config.conf
COPY ./run.sh /run.sh
RUN chmod +x /run.sh

CMD [ "/run.sh" ]
